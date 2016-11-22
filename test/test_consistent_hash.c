#include "test.h"
#include "consistent_hash.c"

#include <assert.h>
#include <limits.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>

clock_t begin;
clock_t delete_clk = 0;

static void __rbtree_print(rbtree_node_t* root) {
  if (root == rbtree_nil)
    return;
  __rbtree_print(root->left);
  printf("%d ", root->key);
  __rbtree_print(root->right);
}

__attribute__((unused)) static void rbtree_print(rbtree_node_t* root) {
  __rbtree_print(root);
  printf("\n");
}

static void statistic(cluster_t* c, int request_num) {
  cluster_node_t** nodes = malloc(sizeof(cluster_node_t*) * c->node_num);
  memset(nodes, 0, sizeof(cluster_node_t*) * c->node_num);
  int* requests = malloc(sizeof(int) * c->node_num);
  memset(requests, 0, sizeof(int) * c->node_num);
  int cnt = 0;

  srand(time(NULL));
  for (int i = 0; i < request_num; ++i) {
    hash_t key = rand() % INT_MAX;
    cluster_node_t* node = cluster_find(c, key);
    assert(node);
    
    int j;
    for (j = 0; j < cnt; ++j) {
      if (nodes[j] == node)
        break;
    }
    ++requests[j];
    if (j == cnt) {
      nodes[j] = node;
      ++cnt;
    }
  }

  printf("total requests: %d\n", request_num);
  printf("total nodes: %d\n", c->node_num);
  printf("average expection: %f%%\n", 100.0 / c->node_num);
  for (int i = 0; i < cnt; ++i) {
    printf("%s: %d: %f%%\n", nodes[i]->name,
        requests[i], 100.0 * requests[i] / request_num);
  }
  printf("\n");
  fflush(stdout);
  free(nodes);
  free(requests);
}

static void test_cluster_remove(cluster_t* c, const char* name) {
  clock_t begin = clock();
  cluster_remove(c, name);
  delete_clk += clock() - begin;
}

static void test(void) {
  cluster_t c;
  cluster_init(&c);
  const char* node_names[] = {
    "192.168.1.87",
    "192.168.1.88",
    "192.168.1.89",
    "192.168.1.90"
  };

  int node_num = sizeof(node_names) / sizeof(const char*);
  for (int i = 0; i < node_num; ++i) {
    printf("add %s\n", node_names[i]);
    fflush(stdout);
    cluster_add(&c, node_names[i]);
  }

  const int request_num = 500000;

  statistic(&c, request_num);

  printf("add %s\n", "192.168.1.91");
  fflush(stdout);
  cluster_add(&c, "192.168.1.91");
  statistic(&c, request_num);
  
  printf("add %s\n", "192.168.1.92");
  fflush(stdout);
  cluster_add(&c, "192.168.1.92");
  printf("add %s\n", "192.168.1.93");
  fflush(stdout);
  cluster_add(&c, "192.168.1.93");
  statistic(&c, request_num);
  
  printf("remove %s\n", "192.168.1.87");
  fflush(stdout);
  test_cluster_remove(&c, "192.168.1.87");
  statistic(&c, request_num);

  //rbtree_print(c.root);
  printf("remove %s\n", "192.168.1.88");
  fflush(stdout);
  test_cluster_remove(&c, "192.168.1.88");
  
  printf("remove %s\n", "192.168.1.89");
  fflush(stdout);
  test_cluster_remove(&c, "192.168.1.89");
  
  printf("remove %s\n", "192.168.1.90");
  fflush(stdout);
  test_cluster_remove(&c, "192.168.1.90");
  
  printf("remove %s\n", "192.168.1.91");
  fflush(stdout);
  test_cluster_remove(&c, "192.168.1.91");
  statistic(&c, request_num);

  cluster_destroy(&c);
}

int test_main(void) {
  begin = clock();
  test();
  printf("total time: %fs\n", 1.0 * (clock() - begin) / CLOCKS_PER_SEC);
  printf("delete time: %fs\n", 1.0 * delete_clk / CLOCKS_PER_SEC);
  return 0;
}
