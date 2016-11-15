#include "consistent_hash.h"

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>

static hash_t hash(const unsigned char* data, size_t len);
static cluster_node_t* cluster_node_new(const char* name);
static inline void cluster_node_delete(cluster_node_t* node);
static void __cluster_remove(cluster_t* c, cluster_node_t* node);

static hash_t hash(const unsigned char* data, size_t len) {
  /*
  md5_state_t state;
  md5_init(&state);
  md5_append(state, data, len);
  md5_byte_t digest[16];
  md5_finish(state, digest);
  */
  static unsigned char md[MD5_DIGEST_LENGTH];  
  MD5(data, len, md);
  hash_t hash = 0;
  for (int i = 0; i < 16; i += 4) {
    hash ^= (*(hash_t*)(md + i));
  }
  return hash & 0x7fffffff;
}

static cluster_node_t* cluster_node_new(const char* name) {
  cluster_node_t* node = malloc(sizeof(cluster_node_t));
  size_t len = strlen(name);
  node->name = name;
  node->name_len = len;
  node->next = NULL;

  unsigned char* data = malloc(sizeof(unsigned char) * (len + 4));
  memcpy(data, name, len);
  for (int i = 0; i < VIRTUAL_NODE_NUM; ++i) {
    memcpy(data + len, &i, 4);
    int key = hash(data, len + 4);
    node->virtual_nodes[i] = rbtree_node_new(key, node);
    //node->virtual_keys[i] = key;
    //node->virtual_nodes[i].node = node;
  }
  free(data);
  return node;
}

static inline void cluster_node_delete(cluster_node_t* node) {
  free(node);
}

void cluster_init(cluster_t* c) {
  c->root = rbtree_nil;
  c->node_num = 0;
  c->node_list = NULL;
}

void cluster_destroy(cluster_t* c) {
  while (c->node_list) {
    cluster_node_t* node = c->node_list;
    c->node_list = node->next;
    __cluster_remove(c, node);
  }
  c->node_list = NULL;
}

/*
 * Add a cluster node
 */
void cluster_add(cluster_t* c, const char* name) {
  cluster_node_t* node = cluster_node_new(name);
  for (int i = 0; i < VIRTUAL_NODE_NUM; ++i) {
    c->root = rbtree_insert(c->root, node->virtual_nodes[i]);
  }
  ++c->node_num;
  node->next = c->node_list;
  c->node_list = node;
}

static void __cluster_remove(cluster_t* c, cluster_node_t* node) {
  for (int i = 0; i < VIRTUAL_NODE_NUM; ++i) {
    c->root = rbtree_delete(c->root, node->virtual_nodes[i]);
  }
  cluster_node_delete(node);
  --c->node_num;
}

/*
 * Remove a cluster node
 */
void cluster_remove(cluster_t* c, const char* name) {
  assert(c->node_num);
  cluster_node_t* dummy = &(cluster_node_t){};
  dummy->next = c->node_list;
  cluster_node_t* p = dummy;
  while (p->next && strncmp(name, p->next->name, p->next->name_len)) {
    p = p->next;
  }
  if (p->next) {
    cluster_node_t* node = p->next;
    p->next = node->next;
    __cluster_remove(c, node);
    c->node_list = dummy->next;
  }
}

/*
 * Find the cluster node that holds the key
 */
cluster_node_t* cluster_find(cluster_t* c, hash_t key) {
  if (c->node_num == 0)
    return NULL;
  rbtree_node_t* lb = rbtree_lowbound(c->root, key);
  if (lb == rbtree_nil) {
    // A ring
    lb = rbtree_minimum(c->root);
  }
  assert(lb != rbtree_nil);
  return lb->val;
}
