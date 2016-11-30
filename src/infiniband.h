#ifndef _NVDS_COMMON_H_
#define _NVDS_COMMON_H_

#include "nvds.h"

namespace nvds {

typedef struct nvds_ib_context {
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
} nvds_ib_context_t;

typedef struct nvds_ib_connection {
  int             	  lid;
  int            	    qpn;
  int             	  psn;
	unsigned 			      rkey;
	unsigned long long 	vaddr;
} nvds_ib_connection_t;

typedef struct nvds_ib_data {
	int									  port;
	int									  ib_port;
	unsigned           	  size;
	int                		tx_depth;
	int 		    					sockfd;
	const char*           server_name;
	nvds_ib_connection_t	local_conn;
	nvds_ib_connection_t  remote_conn;
	struct ibv_device*    ib_dev;
} nvds_ib_data_t;

void nvds_ib_init();
void nvds_ib_rdma_write(nvds_ib_context_t* ctx, nvds_ib_data_t* data);
void nvds_ib_rdma_read(nvds_ib_context_t* ctx, nvds_ib_data_t* data);
void nvds_ib_poll_send(nvds_ib_context_t* ctx);
void nvds_ib_poll_recv(nvds_ib_context_t* ctx);
void nvds_ib_server_exch_info(nvds_ib_data_t* data);
void nvds_ib_client_exch_info(nvds_ib_data_t* data);
void nvds_ib_init_ctx(nvds_ib_context_t* ctx, nvds_ib_data_t* data);

} // namespace nvds

#endif // _NVDS_COMMON_H_
