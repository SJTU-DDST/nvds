#ifndef _NVDS_COMMON_H_
#define _NVDS_COMMON_H_

#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <sys/time.h>
// Interprocess communication access
#include <sys/ipc.h>
// Shared memory facility
#include <sys/shm.h>

#define nvds_error(format, ...) {                         \
  fprintf(stderr, "error: %s: %d: ", __FILE__, __LINE__); \
  fprintf(stderr, format, ## __VA_ARGS__);                \
  fprintf(stderr, "\n");                                  \
  perror("perror: ");                                     \
  exit(-1);                                               \
}

#define nvds_expect(cond, err, ...) { \
  if (!(cond)) {                      \
    nvds_error(err, ## __VA_ARGS__);  \
  }                                   \
}

#define QP_TYPE IBV_QPT_UD

typedef struct nvds_context {
  struct ibv_context*       context;
	struct ibv_pd*            pd;
	struct ibv_mr*            mr;
	struct ibv_cq*            rcq;
	struct ibv_cq*            scq;
	struct ibv_qp*            qp;
	struct ibv_comp_channel*  ch;
	void*                     buf;
	unsigned            	    size;
	int                 	    tx_depth;
	struct ibv_sge      	    sge;
	struct ibv_send_wr  	    wr;
} nvds_context_t;

typedef struct nvds_ib_connection {
  int             	  lid;
  int            	    qpn;
  int             	  psn;
	unsigned 			      rkey;
	unsigned long long 	vaddr;
} nvds_ib_connection_t;

typedef struct nvds_data {
	int									  port;
	int									  ib_port;
	unsigned           	  size;
	int                		tx_depth;
	int 		    					sockfd;
	const char*           server_name;
	nvds_ib_connection_t	local_conn;
	nvds_ib_connection_t  remote_conn;
	struct ibv_device*    ib_dev;
} nvds_data_t;

#endif
