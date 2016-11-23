#include "common.h"

#define RDMA_WRITE_ID   (3)
#define RDMA_READ_ID    (4)

static int page_size;

static void nvds_server_exch_info(nvds_data_t* data);
static void nvds_client_exch_info(nvds_data_t* data);
static void run_server(nvds_context_t* ctx, nvds_data_t* data);
static void run_client(nvds_context_t* ctx, nvds_data_t* data);
static void nvds_init_local_ib_connection(nvds_context_t* ctx,
                                          nvds_data_t* data);
static void nvds_init_ctx(nvds_context_t* ctx, nvds_data_t* data);
static void nvds_set_qp_state_init(struct ibv_qp* qp, nvds_data_t* data);
static void nvds_set_qp_state_rtr(struct ibv_qp* qp, nvds_data_t* data);
static void nvds_set_qp_state_rts(struct ibv_qp* qp, nvds_data_t* data);

static void nvds_rdma_write(nvds_context_t* ctx, nvds_data_t* data);
static void nvds_rdma_read(nvds_context_t* ctx, nvds_data_t* data);
static void nvds_poll_send(nvds_context_t* ctx);
//static void nvds_poll_recv(nvds_context_t* ctx);

static void nvds_set_qp_state_init(struct ibv_qp* qp, nvds_data_t* data) {
  struct ibv_qp_attr attr = {
    .qp_state = IBV_QPS_INIT,
    .pkey_index = 0,
    .port_num = data->ib_port,
    .qp_access_flags = IBV_ACCESS_REMOTE_WRITE
  };

  int modify_flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                     IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
  nvds_expect(ibv_modify_qp(qp, &attr, modify_flags) == 0,
              "ibv_modify_qp() failed");
}

static void nvds_set_qp_state_rtr(struct ibv_qp* qp, nvds_data_t* data) {
  struct ibv_qp_attr attr = {
    .qp_state           = IBV_QPS_RTR,
    .path_mtu           = IBV_MTU_2048,
    .dest_qp_num        = data->remote_conn.qpn,
    .rq_psn             = data->remote_conn.psn,
    .max_dest_rd_atomic = 1,
    .min_rnr_timer      = 12,
    .ah_attr            = {
      .is_global        = 0,
      .dlid             = data->remote_conn.lid,
      .sl               = 1,
      .src_path_bits    = 0,
      .port_num         = data->ib_port
    }
  };
  int modify_flags = IBV_QP_STATE               |
                     IBV_QP_AV                  |
                     IBV_QP_PATH_MTU            |
                     IBV_QP_DEST_QPN            |
                     IBV_QP_RQ_PSN              |
                     IBV_QP_MAX_DEST_RD_ATOMIC  |
                     IBV_QP_MIN_RNR_TIMER;
  nvds_expect(ibv_modify_qp(qp, &attr, modify_flags) == 0,
              "ibv_modify_qp() failed");
}

static void nvds_set_qp_state_rts(struct ibv_qp* qp, nvds_data_t* data) {
	// first the qp state has to be changed to rtr
	nvds_set_qp_state_rtr(qp, data);
	
	struct ibv_qp_attr attr = {
	  .qp_state       = IBV_QPS_RTS,
    .timeout        = 14,
    .retry_cnt      = 7,
    .rnr_retry      = 7,
    .sq_psn         = data->local_conn.psn,
    .max_rd_atomic  = 1
  };
  int modify_flags = IBV_QP_STATE            |
                     IBV_QP_TIMEOUT          |
                     IBV_QP_RETRY_CNT        |
                     IBV_QP_RNR_RETRY        |
                     IBV_QP_SQ_PSN           |
                     IBV_QP_MAX_QP_RD_ATOMIC;
  nvds_expect(ibv_modify_qp(qp, &attr, modify_flags) == 0,
              "ibv_modify_qp() failed");
}

static void nvds_server_exch_info(nvds_data_t* data) {
int listen_fd;
  struct sockaddr_in server_addr;

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  nvds_expect(listen_fd >= 0, "socket() failed");

  int on = 1;
  nvds_expect(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                         (const char*)&on, sizeof(on)) != -1,
              "sotsockopt() failed");
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(data->port);

  nvds_expect(bind(listen_fd, (struct sockaddr*)&server_addr,
                   sizeof(server_addr)) >= 0,
              "bind() failed");
  printf("server listening at port %d\n", data->port);
  listen(listen_fd, 1024);

  int fd = accept(listen_fd, NULL, NULL);
  nvds_expect(fd >= 0, "accept() failed");

  int len = read(fd, &data->remote_conn, sizeof(data->remote_conn));
  nvds_expect(len == sizeof(data->remote_conn), "read() failed");

  len = write(fd, &data->local_conn, sizeof(data->local_conn));
  nvds_expect(len == sizeof(data->local_conn), "write() failed");

  close(fd);
  close(listen_fd);
}

static void nvds_client_exch_info(nvds_data_t* data) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  nvds_expect(fd != -1, "socket() failed");

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(data->port);
  nvds_expect(inet_pton(AF_INET, data->server_name,
                        &server_addr.sin_addr) == 1, 
              "inet_pton() failed");
  nvds_expect(connect(fd, (struct sockaddr*)&server_addr,
                      sizeof(server_addr)) == 0,
              "connect() failed");

  int len = write(fd, &data->local_conn, sizeof(data->local_conn));
  nvds_expect(len == sizeof(data->local_conn), "write() failed");

  len = read(fd, &data->remote_conn, sizeof(data->remote_conn));
  nvds_expect(len == sizeof(data->remote_conn), "read() failed");
  
  close(fd);
}

static void run_server(nvds_context_t* ctx, nvds_data_t* data) {
  nvds_server_exch_info(data);

  // Set queue pair state to RTR(Read to Receive)
  nvds_set_qp_state_rtr(ctx->qp, data);

  // TODO(wgtdkp): poll request from client or do nothing
  while (1) {}
}

static void run_client(nvds_context_t* ctx, nvds_data_t* data) {
  nvds_client_exch_info(data);

  nvds_set_qp_state_rts(ctx->qp, data);

  static const int n = 1024 * 1024;
  for (int i = 0; i < n; ++i) {
    // Step 1: RDMA write to server
    nvds_rdma_write(ctx, data);
    // Step 2: polling if RDMA write done
    nvds_poll_send(ctx);
    // Step 3: RDMA read to server
    nvds_rdma_read(ctx, data);
    // Step 4: polling if RDMA read done
    nvds_poll_send(ctx);
    // Step 5: verify read data
  }

  printf("client exited\n");
  exit(0);
  // Dump statistic info
}

static void nvds_rdma_write(nvds_context_t* ctx, nvds_data_t* data) {
  ctx->sge_list.addr    = (uintptr_t)ctx->buf;
  ctx->sge_list.length  = ctx->size;
  ctx->sge_list.lkey    = ctx->mr->lkey;

  // The address write to
  ctx->wr.wr.rdma.remote_addr = data->remote_conn.vaddr;
  ctx->wr.wr.rdma.rkey        = data->remote_conn.rkey;
  ctx->wr.wr_id               = RDMA_WRITE_ID;
  ctx->wr.sg_list             = &ctx->sge_list;
  ctx->wr.num_sge             = 1;
  ctx->wr.opcode              = IBV_WR_RDMA_WRITE;
  ctx->wr.send_flags          = IBV_SEND_SIGNALED;
  ctx->wr.next                = NULL;

  struct ibv_send_wr* bad_wr;
  nvds_expect(ibv_post_send(ctx->qp, &ctx->wr, &bad_wr) == 0,
              "ibv_post_send() failed");
}

static void nvds_rdma_read(nvds_context_t* ctx, nvds_data_t* data) {
  ctx->sge_list.addr    = (uintptr_t)ctx->buf;
  ctx->sge_list.length  = ctx->size;
  ctx->sge_list.lkey    = ctx->mr->lkey;

  // The address read from
  ctx->wr.wr.rdma.remote_addr = data->remote_conn.vaddr;
  ctx->wr.wr.rdma.rkey        = data->remote_conn.rkey;
  ctx->wr.wr_id               = RDMA_READ_ID;
  ctx->wr.sg_list             = &ctx->sge_list;
  ctx->wr.num_sge             = 1;
  ctx->wr.opcode              = IBV_WR_RDMA_READ;
  ctx->wr.send_flags          = IBV_SEND_SIGNALED;
  ctx->wr.next                = NULL;

  struct ibv_send_wr* bad_wr;
  nvds_expect(ibv_post_send(ctx->qp, &ctx->wr, &bad_wr) == 0,
              "ibv_post_send() failed");
}

static void nvds_poll_send(nvds_context_t* ctx) {
  struct ibv_wc wc;
  while (ibv_poll_cq(ctx->scq, 1, &wc) != 1) {}
  nvds_expect(wc.status == IBV_WC_SUCCESS, "rdma write failed");
  nvds_expect(wc.wr_id == ctx->wr.wr_id, "wr id not matched");
}

/*
static void nvds_poll_recv(nvds_context_t* ctx) {
  struct ibv_wc wc;
  int cnt = ibv_poll_cq(ctx->rcq, 1, &wc);
  nvds_expect(cnt == 1, "ibv_poll_cq() failed");
  nvds_expect(wc.status == IBV_WC_SUCCESS, "rdma read failed");
  nvds_expect(wc.wr_id == ctx->wr.wr_id, "wr id not matched");
}
*/

static void nvds_init_local_ib_connection(nvds_context_t* ctx,
                                          nvds_data_t* data) {
  struct ibv_port_attr attr;
  nvds_expect(ibv_query_port(ctx->context, data->ib_port, &attr) == 0,
              "ibv_query_port() failed");
  data->local_conn.lid = attr.lid;
  data->local_conn.qpn = ctx->qp->qp_num;
  data->local_conn.psn = lrand48() & 0xffffff;
  data->local_conn.rkey = ctx->mr->rkey;
  data->local_conn.vaddr = (uintptr_t)ctx->buf + ctx->size;
}

static void nvds_init_ctx(nvds_context_t* ctx, nvds_data_t* data) {
  memset(ctx, 0, sizeof(nvds_context_t));
  ctx->size = data->size;
  ctx->tx_depth = data->tx_depth;

  // Get IB device
  struct ibv_device** dev_list = ibv_get_device_list(NULL);
  nvds_expect(dev_list, "ibv_get_device_list() failed");
  data->ib_dev = dev_list[0];
  
  // Open IB device
  ctx->context = ibv_open_device(data->ib_dev);
  nvds_expect(ctx->context, "ibv_open_device() failed");

  // Alloc protection domain
  ctx->pd = ibv_alloc_pd(ctx->context);
  nvds_expect(ctx->pd, "ibv_alloc_pd() failed");

  // Alloc buffer for write
  ctx->buf = memalign(page_size, ctx->size);
  memset(ctx->buf, 0, ctx->size);

  // Register memory region
  int access = IBV_ACCESS_LOCAL_WRITE |
               IBV_ACCESS_REMOTE_READ |
				       IBV_ACCESS_REMOTE_WRITE;
  ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, ctx->size, access);
  nvds_expect(ctx->mr, "ibv_reg_mr() failed");

  // Create completion channel
  ctx->ch = ibv_create_comp_channel(ctx->context);
  nvds_expect(ctx->ch, "ibv_create_comp_channel() failed");

  // Create complete queue
  ctx->rcq = ibv_create_cq(ctx->context, 1, NULL, NULL, 0);
  nvds_expect(ctx->rcq, "ibv_create_cq() failed");
  ctx->scq = ibv_create_cq(ctx->context, ctx->tx_depth, ctx, ctx->ch, 0);
  nvds_expect(ctx->rcq, "ibv_create_cq() failed");  

  // Create & init queue apir
  struct ibv_qp_init_attr qp_init_attr = {
    .send_cq = ctx->scq,
    .recv_cq = ctx->rcq,
    .qp_type = IBV_QPT_RC,
    .cap = {
      .max_send_wr = ctx->tx_depth,
      .max_recv_wr = 1,
      .max_send_sge = 1,
      .max_recv_sge = 1,
      .max_inline_data = 0
    }
  };
  ctx->qp = ibv_create_qp(ctx->pd, &qp_init_attr);
  nvds_expect(ctx->qp, "ibv_create_qp() failed");
  nvds_set_qp_state_init(ctx->qp, data);
}

int main(int argc, const char* argv[]) {
  nvds_context_t ctx;
  nvds_data_t data = {
    .port         = 5500,
    .ib_port      = 1,
    .size         = 65536,
    .tx_depth     = 100,
    .server_name  = NULL,
    .ib_dev       = NULL
  };

  nvds_expect(argc <= 2, "too many arguments");
  int is_client = 0;
  if (argc == 2) {
    is_client = 1;
    data.server_name = argv[1];
  }

  pid_t pid = getpid();
  srand48(pid * time(NULL));
  page_size = sysconf(_SC_PAGESIZE);
  nvds_init_ctx(&ctx, &data);
  nvds_init_local_ib_connection(&ctx, &data);

  if (is_client) {
    run_client(&ctx, &data);
  } else {
    run_server(&ctx, &data);
  }

  return 0;
}