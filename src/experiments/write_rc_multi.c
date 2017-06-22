#include "write_rc_multi.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define RDMA_WRITE_ID   (3)
#define RDMA_READ_ID    (4)
#define RDMA_MAX_LEN  (128 * 1024)
//#define RDMA_WRITE_LEN  (64)
//#define RDMA_READ_LEN   RDMA_WRITE_LEN

static int page_size;

static void nvds_server_exch_info(nvds_data_t* data);
static void nvds_client_exch_info(nvds_data_t* data);
static void nvds_run_server(nvds_context_t* ctx_arr, nvds_data_t* data_arr, uint32_t num);
static void nvds_run_client(nvds_context_t* ctx_arr, nvds_data_t* data_arr, uint32_t num);
static void nvds_init_local_ib_connection(nvds_context_t* ctx_arr, nvds_data_t* data_arr, uint32_t num);
static void nvds_init_ctx(nvds_context_t* ctx_arr, nvds_data_t* data_arr, uint32_t num);
static void nvds_set_qp_state_init(struct ibv_qp* qp, nvds_data_t* data);
static void nvds_set_qp_state_rtr(struct ibv_qp* qp, nvds_data_t* data);
static void nvds_set_qp_state_rts(struct ibv_qp* qp, nvds_data_t* data);

static void nvds_rdma_write(nvds_context_t* ctx, nvds_data_t* data, uint32_t len);
//static void nvds_rdma_read(nvds_context_t* ctx, nvds_data_t* data);
static void nvds_poll_send(nvds_context_t* ctx);
//static void nvds_poll_recv(nvds_context_t* ctx);
//static void nvds_post_send(nvds_context_t* ctx, nvds_data_t* data);
//static void nvds_post_recv(nvds_context_t* ctx, nvds_data_t* data);

static void nvds_print_ib_connection(nvds_ib_connection_t* c) {
  printf("lid: %d\n", c->lid);
  printf("qpn: %d\n", c->qpn);
  printf("psn: %d\n", c->psn);
  printf("rkey: %u\n", c->rkey);
  printf("vaddr: %llu\n", c->vaddr);
}

static void nvds_set_qp_state_init(struct ibv_qp* qp, nvds_data_t* data) {
  struct ibv_qp_attr attr = {
    .qp_state = IBV_QPS_INIT,
    .pkey_index = 0,
    .port_num = data->ib_port,
    .qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                       IBV_ACCESS_REMOTE_READ |
                       IBV_ACCESS_REMOTE_WRITE
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
      .is_global      = 0,
      .dlid           = data->remote_conn.lid,
      .sl             = 1,
      .src_path_bits  = 0,
      .port_num       = data->ib_port
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
  printf("information exchanged\n");
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
  //printf("information exchanged\n");
}

static void nvds_run_server(nvds_context_t* ctx_arr, nvds_data_t* data_arr, uint32_t num) {
  for (uint32_t i = 0; i < num; ++i) {
    nvds_context_t* ctx = &ctx_arr[i];
    nvds_data_t* data = &data_arr[i];
    nvds_server_exch_info(data);
    nvds_set_qp_state_rts(ctx->qp, data);
    for (int i = 0; i < ctx->buf_size; ++i) {
      ctx->buf[i] = 0;
    }
  }
  
  int i = 0, j = 0;
  while (true) {
    nvds_context_t* ctx = &ctx_arr[j];
    nvds_data_t* data = &data_arr[j];
    while (*ctx->buf == 0) {}
    *ctx->buf = 0;
    //printf("%d: received\n", ++i);
    nvds_rdma_write(ctx, data, 1);
    //nvds_poll_send(ctx);
    ++j;
    j %= num;
  }
}

static inline int64_t time_diff(struct timespec* lhs, struct timespec* rhs) {
  return (lhs->tv_sec - rhs->tv_sec) * 1000000000 + 
         (lhs->tv_nsec - rhs->tv_nsec);
}

static void nvds_run_client(nvds_context_t* ctx_arr, nvds_data_t* data_arr, uint32_t num) {
  for (uint32_t i = 0; i < num; ++i) {
    nvds_context_t* ctx = &ctx_arr[i];
    nvds_data_t* data = &data_arr[i];
    nvds_client_exch_info(data);
    nvds_set_qp_state_rts(ctx->qp, data);
    for (int i = 0; i < ctx->buf_size; ++i) {
      ctx->buf[i] = 1;
    }
    usleep(1000);
  }
  //printf("client side: \n");
  //printf("local connection: \n");
  //nvds_print_ib_connection(&data->local_conn);
  //printf("remote connection: \n");  
  //nvds_print_ib_connection(&data->remote_conn);
  usleep(1000);
  

  static const int n = 1000 * 1000;
  int j = 0;
  for (int32_t len = 128; len <= 128; len <<= 1) {
    struct timespec begin, end;
    assert(clock_gettime(CLOCK_REALTIME, &begin) == 0);
    for (int i = 0; i < n; ++i) {
      nvds_context_t* ctx = &ctx_arr[j];
      nvds_data_t* data = &data_arr[j];
      nvds_rdma_write(ctx, data, len);
      //nvds_poll_send(ctx);
      while (*ctx->buf == 1) {}
      *ctx->buf = 1;
      ++j;
      j %= num;
    }
    assert(clock_gettime(CLOCK_REALTIME, &end) == 0);
    int64_t t = time_diff(&end, &begin);

    printf("%d %d, %f\n", num, len, t / 1000000.0 / 1000 / 2);
  }
  exit(0);
}

static void nvds_rdma_write(nvds_context_t* ctx, nvds_data_t* data, uint32_t len) {
  static int k = 0;

  ctx->sge.addr    = (uintptr_t)ctx->buf;
  ctx->sge.length  = len;
  ctx->sge.lkey    = ctx->mr->lkey;

  // The address write to
  ctx->wr.wr.rdma.remote_addr = data->remote_conn.vaddr;
  ctx->wr.wr.rdma.rkey        = data->remote_conn.rkey;
  ctx->wr.wr_id               = RDMA_WRITE_ID;
  ctx->wr.sg_list             = &ctx->sge;
  ctx->wr.num_sge             = 1;
  ctx->wr.opcode              = IBV_WR_RDMA_WRITE;
  ctx->wr.send_flags          = k / NUM_QP == 99 ? IBV_SEND_SIGNALED : 0;
  ctx->wr.next                = NULL;

  struct ibv_send_wr* bad_wr;
  nvds_expect(ibv_post_send(ctx->qp, &ctx->wr, &bad_wr) == 0,
              "ibv_post_send() failed");
  if (k / NUM_QP == 99) {
    nvds_poll_send(ctx);
  }
  ++k;
  k %= NUM_QP * 100;
}

/*
static void nvds_rdma_read(nvds_context_t* ctx, nvds_data_t* data) {
  ctx->sge.addr    = (uintptr_t)ctx->buf + RDMA_WRITE_LEN;
  ctx->sge.length  = RDMA_READ_LEN;
  ctx->sge.lkey    = ctx->mr->lkey;

  // The address read from
  ctx->wr.wr.rdma.remote_addr = data->remote_conn.vaddr;
  ctx->wr.wr.rdma.rkey        = data->remote_conn.rkey;
  ctx->wr.wr_id               = RDMA_READ_ID;
  ctx->wr.sg_list             = &ctx->sge;
  ctx->wr.num_sge             = 1;
  ctx->wr.opcode              = IBV_WR_RDMA_READ;
  ctx->wr.send_flags          = IBV_SEND_SIGNALED;
  ctx->wr.next                = NULL;

  struct ibv_send_wr* bad_wr;
  nvds_expect(ibv_post_send(ctx->qp, &ctx->wr, &bad_wr) == 0,
              "ibv_post_send() failed");
}
*/
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

/*
static void nvds_post_send(nvds_context_t* ctx, nvds_data_t* data) {

}

static void nvds_post_recv(nvds_context_t* ctx, nvds_data_t* data) {

}
*/

static void nvds_init_local_ib_connection(nvds_context_t* ctx_arr, nvds_data_t* data_arr, uint32_t num) {
  for (uint32_t i = 0; i < num; ++i) {
    nvds_context_t* ctx = &ctx_arr[i];
    nvds_data_t* data = &data_arr[i];
    struct ibv_port_attr attr;
    nvds_expect(ibv_query_port(ctx->context, data->ib_port, &attr) == 0,
                "ibv_query_port() failed");
    data->local_conn.lid = attr.lid;
    data->local_conn.qpn = ctx->qp->qp_num;
    data->local_conn.psn = 33; //lrand48() & 0xffffff;
    data->local_conn.rkey = ctx->mr->rkey;
    data->local_conn.vaddr = (uintptr_t)ctx->buf;
  }
}

static void nvds_init_ctx(nvds_context_t* ctx_arr, nvds_data_t* data_arr, uint32_t num) {
  // Get IB device

  for (int i = 0; i < num; ++i) {
    nvds_context_t* ctx = &ctx_arr[i];
    nvds_data_t* data = &data_arr[i];

    struct ibv_device** dev_list = ibv_get_device_list(NULL);
    nvds_expect(dev_list, "ibv_get_device_list() failed");
    data->ib_dev = dev_list[0];

    memset(ctx, 0, sizeof(nvds_context_t));
    ctx->buf_size = data->size;
    ctx->tx_depth = data->tx_depth;

    // Open IB device
    if (i == 0) {
      ctx->context = ibv_open_device(data->ib_dev);
      nvds_expect(ctx->context, "ibv_open_device() failed");

      // Alloc protection domain
      ctx->pd = ibv_alloc_pd(ctx->context);
      nvds_expect(ctx->pd, "ibv_alloc_pd() failed");
    } else {
      ctx->context = ctx[-1].context;
      ctx->pd = ctx[-1].pd;
    }

    // Alloc buffer for write
    ctx->buf = memalign(page_size, ctx->buf_size);
    memset(ctx->buf, 0, ctx->buf_size);

    // Register memory region
    int access = IBV_ACCESS_LOCAL_WRITE |
                IBV_ACCESS_REMOTE_READ |
                IBV_ACCESS_REMOTE_WRITE;
    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, ctx->buf_size, access);
    nvds_expect(ctx->mr, "ibv_reg_mr() failed");

    // Create completion channel
    //ctx->ch = ibv_create_comp_channel(ctx->context);
    //nvds_expect(ctx->ch, "ibv_create_comp_channel() failed");

    // Create complete queue
    ctx->rcq = ibv_create_cq(ctx->context, 1, NULL, NULL, 0);
    nvds_expect(ctx->rcq, "ibv_create_cq() failed");
    //ctx->scq = ibv_create_cq(ctx->context, ctx->tx_depth, ctx, ctx->ch, 0);
    ctx->scq = ibv_create_cq(ctx->context, ctx->tx_depth, NULL, NULL, 0);
    nvds_expect(ctx->rcq, "ibv_create_cq() failed");  

    // Create & init queue apir
    struct ibv_qp_init_attr qp_init_attr = {
      .send_cq = ctx->scq,
      .recv_cq = ctx->rcq,
      .qp_type = QP_TYPE,
      .sq_sig_all = 0,
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
}

int main(int argc, const char* argv[]) {
  nvds_expect(argc <= 2, "too many arguments");
  int is_client = argc == 2;

  nvds_data_t data_arr[NUM_QP];
  for (uint32_t i = 0; i < NUM_QP; ++i) {
    data_arr[i] = (nvds_data_t) {
      .port         = 5500,
      .ib_port      = 1,
      .size         = RDMA_MAX_LEN,
      .tx_depth     = 100,
      .server_name  = NULL,
      .ib_dev       = NULL,
      .server_name = argc == 2 ? argv[1] : NULL
    };
  }

  pid_t pid = getpid();
  srand48(pid * time(NULL));
  page_size = sysconf(_SC_PAGESIZE);

  nvds_context_t ctx_arr[NUM_QP];
  nvds_init_ctx(ctx_arr, data_arr, NUM_QP);
  nvds_init_local_ib_connection(ctx_arr, data_arr, NUM_QP);

  if (is_client) {
    nvds_run_client(ctx_arr, data_arr, NUM_QP);
  } else {
    nvds_run_server(ctx_arr, data_arr, NUM_QP);
  }

  return 0;
}
