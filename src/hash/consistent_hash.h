#ifndef _CONSISTENT_HASH_
#define _CONSISTENT_HASH_

#include "rbtree.h"

#include <stddef.h>

#define VIRTUAL_NODE_NUM  (128)

typedef int hash_t;
typedef struct cluster_node cluster_node_t;
typedef struct virtual_node virtual_node_t;
struct virtual_node {
  cluster_node_t* node;
};

struct cluster_node {
  rbtree_node_t* virtual_nodes[VIRTUAL_NODE_NUM];
  const char* name;
  size_t name_len;
  cluster_node_t* next;
  // Data index
};

typedef struct cluster {
  rbtree_node_t* root;
  int node_num;
  cluster_node_t* node_list;
  // coordinator_t coord;
} cluster_t;

void cluster_init(cluster_t* c);
void cluster_destroy(cluster_t* c);
void cluster_add(cluster_t* c, const char* name);
void cluster_remove(cluster_t* c, const char* name);
cluster_node_t* cluster_find(cluster_t* c, hash_t key);

#endif
