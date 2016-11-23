#include "infiniband.h"

#define AREA_SIZE         (1024 * 1024)
#define REQ_AREA_SHM_KEY  (3185)
#define RESP_AREA_SHM_KEY (3186)
#define QUEUE_SIZE        (1024)

#define IB_PHYS_PORT (1)

#define USE_UC 1				  // Use UC for requests. If 0, RC is ued
#define USE_INLINE 1			// Use WQE inlining for requests and responses
#define USE_HUGEPAGE 1

/*
 * Abbreviations:
 *  WR: work request
 *  PD: proetction domain
 *  CQ: completion queue
 *  WQ: work queue (send, receive, completion) 
 *  QP: queue pair
 *  SRQ: shared receive queue
 *  AH: address handle
 *  MR: memory region (create L_Key(local key) and R_Key(remote key) for authentication)
 *  IB: infiniband
 *  CA: channel adapter (NIC)
 *  HCA: host channel adapter
 *  TCA: target channel adapter
 *  SGE: Scatter/Gather Element (associated with a WR)
 */

/*
 * To send or receive messages, Work Requests(WRs) are placed onto a QP.
 * There are send work requests and recieve work requests. When processing
 * is completed, a Work Completion(WC) entry is optionally placed onto a
 * Compeltion Queue(CQ) associated with the work queue.

 * The R_Key(Remote Key) of a memory region must be sent to the peer, so that 
 * it can directly access the memory region(RDMA Read, RDMA Write). 
 */

struct stag {								// An "stag" identifying an RDMA region
	uint64_t buf;
	uint32_t rkey;
	uint32_t size;
};

static uint16_t server_port = 5500;

volatile char* req_area = NULL;
//volatile char* server_resp_area;
struct ibv_mr* req_mr = NULL;
struct ibv_pd* pd = NULL;
struct ibv_ah* ah = NULL;

//volatile char* client_req_area = NULL;
//volatile char* client_resp_area = NULL;
//struct ibv_mr* client_req_mr = NULL;
//struct ibv_mr* client_resp_mr = NULL;
//struct ibv_pd* client_pd = NULL;

struct ibv_port_attr local_port_attr;
struct ibv_port_attr remote_port_attr;
uint16_t remote_lid = 0;
uint16_t local_lid = 0;

struct stag remote_stag;

struct ibv_qp* qp = NULL;
struct ibv_cp* cp = NULL;

static inline uint16_t nvds_get_local_lid(struct ibv_context* ctx) {
  struct ibv_port_attr attr;
  if (ibv_query_port(ctx, IB_PHYS_PORT, &attr) != 0)
    return 0;
  return attr.lid;
}

// If there are multiple ib devices, return the first
struct ibv_context* nvds_get_ib_device() {
  int n = 0;
  struct ibv_device** ib_dev_list;
  ib_dev_list = ibv_get_device_list(&n);
  if (ib_dev_list == NULL) {
    nvds_error("no available infiniband devices!");
  } else {
    printf("found %d infiniband device\n", n);
  }

  struct ibv_device* ib_dev = ib_dev_list[0];

  struct ibv_context* ctx;
	struct ibv_pd* pd; // Protection domain
  ctx = ibv_open_device(ib_dev);
  nvds_expect(ctx, "cannot get context");
  return ctx;
  //pd = ibv_alloc_pd(ctx);
  //nvds_expect(pd, "cannot allocate PD");
  //if (pd) {}
}

void nvds_server_setup_buffer(struct ibv_context* ctx) {
  //int sid = shmget(REQ_AREA_SHM_KEY, AREA_SIZE,
  //                 IPC_CREAT | 0666 | SHM_HUGETLB);
  //nvds_expect(sid != -1, "shmget(), size: %d failed", AREA_SIZE);
  server_req_area = malloc(AREA_SIZE);
  nvds_expect(server_req_area, "malloc() failed");
  memset(server_req_area, 0, AREA_SIZE);
  
  server_pd = ibv_alloc_pd(ctx);
  nvds_expect(server_pd, "ibv_alloc_pd() failed");
  int access_flags = IBV_ACCESS_LOCAL_WRITE |
                     IBV_ACCESS_REMOTE_READ |
				             IBV_ACCESS_REMOTE_WRITE;
  server_req_mr = ibv_reg_mr(server_pd, server_req_area,
                             AREA_SIZE, access_flags);
  nvds_expect(server_req_mr, "ibv_reg_mr() failed");
}

void nvds_client_setup_buffer(struct ibv_context* ctx) {
  client_pd = ibv_alloc_pd(ctx);
  nvds_expect(client_pd, "ibv_alloc_pd() failed");

  client_req_area = memalign(4096, AREA_SIZE);
  nvds_expect(client_req_area, "memalign() failed");
  memset(client_req_area, 0, AREA_SIZE);

  int access_flags = IBV_ACCESS_LOCAL_WRITE |
                     IBV_ACCESS_REMOTE_READ |
				             IBV_ACCESS_REMOTE_WRITE;
  client_req_mr = ibv_reg_mr(client_pd, client_req_area,
                             AREA_SIZE, access_flags);
  nvds_expect(client_req_mr, "ibv_reg_mr() failed");

  client_resp_area = memalign(4096, AREA_SIZE);
  nvds_expect(client_resp_area, "memalign() failed");
  memset(client_resp_area, 0, AREA_SIZE);
  client_resp_mr = ibv_reg_mr(client_pd, client_resp_area,
                              AREA_SIZE, access_flags);
  nvds_expect(client_resp_mr, "ibv_reg_mr() failed");
}

int nvds_connect_ctx(struct context* ctx) {
	struct ibv_qp_attr conn_attr = {
		.qp_state			= IBV_QPS_RTR,
		.path_mtu			= IBV_MTU_4096,
		.dest_qp_num	= dest.qpn,
		.rq_psn				= dest.psn,
		.ah_attr      = {
      .is_global      = 0,
      .dlid           = remote_lid,
      .sl             = 0,
      .src_path_bits  = 0,
      .port_num       = 1
    }
	};
  struct ibv_ah_attr ;

  int rtr_flags = IBV_QP_STATE | IBV_QP_AV |
                  IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN;
  

}

void nvds_server_exch_info(struct context* ctx) {
  int listen_fd;
  struct sockaddr_in server_addr;

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  nvds_expect(listen_fd >= 0, "socket() failed");

  int on = 1;
  nvds_expect(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                         (const char*)&on, sizeof(on)) != -1,
              "sotsockopt() failed");
  mmeset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(server_port);

  nvds_expect(bind(listen_fd, (struct sockaddr*)&server_addr,
                   sizeof(server_addr)) >= 0,
              "bind() failed");
  printf("server listening at port %d\n", server_port);
  listen(listen_fd, 1024);

  while (1) {
    int fd = accept(listen_fd, NULL, NULL);
    nvds_expect(fd >= 0, "accept() failed");
    struct stag stag;
    stag.buf  = (uintptr_t)server_req_area;
    stag.rkey = server_req_mr->rkey;
    stag.size = AREA_SIZE;

    nvds_expect(write(fd, &stag, sizeof(stag)) >= 0,
                "send server stag failed");
    nvds_expect(read(fd, &remote_lid, sizeof(remote_lid)) >= 0,
                "read client lid failed");
    nvds_expect(connect_ctx(ctx) != 0, "cannot connect to remote QP");

    close(fd);
    close(listen_fd);
    return;
  }
}

void nvds_client_exch_info(struct context* ctx) {
  int fd;
  struct sockaddr_in server_addr;
  const char host[64] = "192.168.99.11";
  fd = socket(AF_INET, SOCK_STREAM, 0);
  nvds_expect(fd >= 0, "socket() failed");
  struct hostent* server = gethostbyaddr(host,
                                         strlen(host), AF_INET);
  nvds_expect(server, "cannot find server %s", host);
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  nvds_expect(inet_pton(AF_INET, host, &server_addr.sin_addr) == 1, 
              "inet_pton() failed");
  nvds_expect(connect(fd, (struct sockaddr*)&server_addr,
                      sizeof(server_addr)) == 0,
              "connect() failed");
  
  nvds_expect(read(fd, &remote_stag, sizeof(remote_stag)) >= 0,
              "read stag failed");
  nvds_expect(write(fd, &local_lid, sizeof(local_lid)) >= 0,
              "write lid failed");
  close(fd);
}

void nvds_startup_server() {
  // Step 1: get the infiniband device
  struct ibv_context* ctx = nvds_get_ib_device();

  // Step 2: create protection domain and 
  //  register memory regions for request and response
  nvds_server_setup_buffer(ctx);
  
  // Step 3: create complete queue and queue pair
  cq = ibv_create_cq(ctx, QUEUE_SIZE, NULL, NULL, 0);
  nvds_expect(cp, "ibv_create_cq() failed");
  struct ibv_qp_init_attr qp_init_attr = {
    .send_cq = cq,
    .recv_cq = cq,
    .qp_type = IBV_QPT_RC,
    .cap = {
      .max_send_wr  = QUEUE_SIZE,
      .max_recv_wr  = 1,
      .max_send_sge = 1,
      .max_recv_sge = 1,
      .max_inline_data = 256
    }
  };
  qp = ibv_create_qp(pd, &qp_init_attr);
  nvds_expect(qp, "ibv_create_qp() failed");

  // Step 4: contact remote peer, exchange informations
  local_lid = nvds_get_local_lid(ctx);
  // Send memory region info, get remote lid
  nvds_server_exch_info(ctx);

  // Step 5: create address handle
  struct ibv_ah_attr ah_attr = {
    .ls_global      = 0,
    .dlid           = remote_lid,
    .sl             = 0,
    .src_path_bits  = 0,
    .port_num       = IB_PHYS_PORT
  };
  server_ah = ibv_create_ah(server_pd, &ah_attr);
  nvds_expect(server_ah, "ibv_create_ah() failed");

  // Step 6: post receive


  // Step 7: poll complete queue
  while (1) {

  }


}

void nvds_post_send(struct context* ctx) {
  struct ibv_sge sg = {
    .addr   = (uintptr_t)req_area;
    .length = AREA_SIZE,
    .lkey   = req_mr->lkey
  };

  struct ibv_send_wr wr;
  struct ibv_send_wr* bad_wr;
  memset(&wr, 0, sizeof(wr));
  wr.wr_id      = 0;
  wr.sg_list    = &sg;
  wr.num_sge    = 1;
  wr.opcode     = IBV_WR_SEND;
  wr.send_flags = IBV_SEND_SIGNALED;

  nvds_expect(ibv_post_send(qp, &wr, &bad_wr) == 0,
              "ibv_post_send() failed");
}
