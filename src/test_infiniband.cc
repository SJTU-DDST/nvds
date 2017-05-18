#include "infiniband.h"
#include <gtest/gtest.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

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

using namespace std;
using namespace nvds;

#define nvds_error(format, ...) {                         \
  fprintf(stderr, "error: %s: %d: ", __FILE__, __LINE__); \
  fprintf(stderr, format, ## __VA_ARGS__);                \
  fprintf(stderr, "\n");                                  \
  exit(-1);                                               \
}

#define nvds_expect(cond, err, ...) { \
  if (!(cond)) {                      \
    nvds_error(err, ## __VA_ARGS__);  \
  }                                   \
}

#define BUF_SIZE    (1024)
#define QP_TYPE     IBV_QPT_RC
#define IB_PORT     (1)
#define DEFAULT_PSN (33)

typedef struct nvds_context {
  struct ibv_context*       context;
	struct ibv_pd*            pd;
	struct ibv_mr*            mr;
	struct ibv_cq*            rcq;
	struct ibv_cq*            scq;
	struct ibv_qp*            qp;
	struct ibv_comp_channel*  ch;
	char*                     buf;
} nvds_context_t;

typedef struct nvds_ib_connection {
  int             	  lid;
  int            	    qpn;
  int             	  psn;
	unsigned 			      rkey;
	unsigned long long 	vaddr;
} nvds_ib_connection_t;

#define RDMA_WRITE_ID   (3)
#define RDMA_READ_ID    (4)
#define RDMA_WRITE_LEN  (64)
#define RDMA_READ_LEN   RDMA_WRITE_LEN

static int page_size;
nvds_ib_connection_t	conn[2];

static void nvds_run_server(nvds_context_t* ctx, nvds_ib_connection_t* remote_conn);
static void nvds_run_client(nvds_context_t* ctx, nvds_ib_connection_t* remote_conn);
static void nvds_init_local_ib_connection(nvds_context_t* ctx,
                                          nvds_ib_connection_t* local_conn);
static void nvds_init_ctx(nvds_context_t* ctx);
static void nvds_set_qp_state_init(struct ibv_qp* qp);
static void nvds_set_qp_state_rtr(struct ibv_qp* qp, nvds_ib_connection_t* remote_conn);
static void nvds_set_qp_state_rts(struct ibv_qp* qp, nvds_ib_connection_t* remote_conn);

static void nvds_rdma_write(nvds_context_t* ctx, nvds_ib_connection_t* remote_conn);
static void nvds_poll_send(nvds_context_t* ctx);

static void copy_info_to_conn(nvds_ib_connection_t* conn, Infiniband::QueuePairInfo* info) {
  conn->lid = info->lid;
  conn->qpn = info->qpn;
  conn->psn = info->psn;
  conn->rkey = info->rkey;
  conn->vaddr = info->vaddr;
}

static void copy_conn_to_info(Infiniband::QueuePairInfo* info, nvds_ib_connection_t* conn) {
  info->lid = conn->lid;
  info->qpn = conn->qpn;
  info->psn = conn->psn;
  info->rkey = conn->rkey;
  info->vaddr = conn->vaddr;
}

static void nvds_print_ib_connection(nvds_ib_connection_t* c) {
  printf("lid: %d\n", c->lid);
  printf("qpn: %d\n", c->qpn);
  printf("psn: %d\n", c->psn);
  printf("rkey: %u\n", c->rkey);
  printf("vaddr: %llu\n", c->vaddr);
}

static void nvds_set_qp_state_init(struct ibv_qp* qp) {
  struct ibv_qp_attr attr;
  attr.qp_state = IBV_QPS_INIT;
  attr.pkey_index = 0;
  attr.port_num = Infiniband::kPort;
  attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                         IBV_ACCESS_REMOTE_READ |
                         IBV_ACCESS_REMOTE_WRITE;

  int modify_flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                     IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
  nvds_expect(ibv_modify_qp(qp, &attr, modify_flags) == 0,
              "ibv_modify_qp() failed");
}

static void nvds_set_qp_state_rtr(struct ibv_qp* qp, nvds_ib_connection_t* remote_conn) {
  struct ibv_qp_attr attr;
  attr.qp_state           = IBV_QPS_RTR;
  attr.path_mtu           = IBV_MTU_4096;
  attr.dest_qp_num        = remote_conn->qpn;
  attr.rq_psn             = remote_conn->psn;
  attr.max_dest_rd_atomic = 1;
  attr.min_rnr_timer      = 12;
  attr.ah_attr.is_global  = 0;
  attr.ah_attr.dlid       = remote_conn->lid;
  attr.ah_attr.sl             = 0;//1;
  attr.ah_attr.src_path_bits  = 0;
  attr.ah_attr.port_num       = 1;

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

static void nvds_set_qp_state_rts(struct ibv_qp* qp, nvds_ib_connection_t* remote_conn) {
	// first the qp state has to be changed to rtr
	nvds_set_qp_state_rtr(qp, remote_conn);

	struct ibv_qp_attr attr;
  attr.qp_state       = IBV_QPS_RTS;
  attr.timeout        = 14;
  attr.retry_cnt      = 7;
  attr.rnr_retry      = 7;
  attr.sq_psn         = DEFAULT_PSN;
  attr.max_rd_atomic  = 1;

  int modify_flags = IBV_QP_STATE            |
                     IBV_QP_TIMEOUT          |
                     IBV_QP_RETRY_CNT        |
                     IBV_QP_RNR_RETRY        |
                     IBV_QP_SQ_PSN           |
                     IBV_QP_MAX_QP_RD_ATOMIC;
  nvds_expect(ibv_modify_qp(qp, &attr, modify_flags) == 0,
              "ibv_modify_qp() failed");
}

static void nvds_rdma_write(nvds_context_t* ctx, nvds_ib_connection_t* remote_conn) {
  struct ibv_sge sge;
  sge.addr    = (uintptr_t)ctx->buf;
  sge.length  = RDMA_WRITE_LEN;
  sge.lkey    = ctx->mr->lkey;

  struct ibv_send_wr wr;
  // The address write to
  wr.wr.rdma.remote_addr = remote_conn->vaddr;
  wr.wr.rdma.rkey        = remote_conn->rkey;
  wr.wr_id               = RDMA_WRITE_ID;
  wr.sg_list             = &sge;
  wr.num_sge             = 1;
  wr.opcode              = IBV_WR_RDMA_WRITE;
  wr.send_flags          = IBV_SEND_SIGNALED;
  wr.next                = NULL;

  struct ibv_send_wr* bad_wr;
  nvds_expect(ibv_post_send(ctx->qp, &wr, &bad_wr) == 0,
              "ibv_post_send() failed");
}

static void nvds_poll_send(nvds_context_t* ctx) {
  struct ibv_wc wc;
  int r;
  while ((r = ibv_poll_cq(ctx->scq, 1, &wc)) != 1) {}
  //nvds_expect(wc.status == IBV_WC_SUCCESS, "rdma write failed");
  if (wc.status != IBV_WC_SUCCESS) {
    printf("rdma write failed, status = %d\n", wc.status);
    perror("ibv_poll_cq");
    abort();
  }
}

static void nvds_init_local_ib_connection(nvds_context_t* ctx,
                                          nvds_ib_connection_t* local_conn) {
  struct ibv_port_attr attr;
  nvds_expect(ibv_query_port(ctx->context, IB_PORT, &attr) == 0,
              "ibv_query_port() failed");
  local_conn->lid = attr.lid;
  local_conn->qpn = ctx->qp->qp_num;
  local_conn->psn = Infiniband::QueuePair::kDefaultPsn; //lrand48() & 0xffffff;
  local_conn->rkey = ctx->mr->rkey;
  local_conn->vaddr = (uintptr_t)ctx->buf;
}

static void nvds_init_ctx(nvds_context_t* ctx) {
  memset(ctx, 0, sizeof(nvds_context_t));

  // Get IB device
  struct ibv_device** dev_list = ibv_get_device_list(NULL);
  nvds_expect(dev_list, "ibv_get_device_list() failed");

  // Open IB device
  ctx->context = ibv_open_device(dev_list[0]);
  nvds_expect(ctx->context, "ibv_open_device() failed");

  // Alloc protection domain
  ctx->pd = ibv_alloc_pd(ctx->context);
  nvds_expect(ctx->pd, "ibv_alloc_pd() failed");

  // Alloc buffer for write
  ctx->buf = (char*)memalign(4096, BUF_SIZE);
  memset(ctx->buf, 0, BUF_SIZE);

  // Register memory region
  int access = IBV_ACCESS_LOCAL_WRITE |
               IBV_ACCESS_REMOTE_READ |
				       IBV_ACCESS_REMOTE_WRITE;
  ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, BUF_SIZE, access);
  nvds_expect(ctx->mr, "ibv_reg_mr() failed");

  // Create completion channel
  //ctx->ch = ibv_create_comp_channel(ctx->context);
  //nvds_expect(ctx->ch, "ibv_create_comp_channel() failed");

  // Create complete queue
  ctx->rcq = ibv_create_cq(ctx->context, 128, NULL, NULL, 0);
  nvds_expect(ctx->rcq, "ibv_create_cq() failed");
  //ctx->scq = ibv_create_cq(ctx->context, ctx->tx_depth, ctx, ctx->ch, 0);
  ctx->scq = ibv_create_cq(ctx->context, 128, NULL, NULL, 0);
  nvds_expect(ctx->rcq, "ibv_create_cq() failed");  

  // Create & init queue apir
  struct ibv_qp_init_attr qp_init_attr;
  qp_init_attr.send_cq = ctx->scq;
  qp_init_attr.recv_cq = ctx->rcq;
  qp_init_attr.qp_type = QP_TYPE;
  qp_init_attr.cap.max_send_wr = 128;
  qp_init_attr.cap.max_recv_wr = 128;
  qp_init_attr.cap.max_send_sge = Infiniband::kMaxSendSge;
  qp_init_attr.cap.max_recv_sge = Infiniband::kMaxRecvSge;
  qp_init_attr.cap.max_inline_data = Infiniband::kMaxInlineData;

  ctx->qp = ibv_create_qp(ctx->pd, &qp_init_attr);
  nvds_expect(ctx->qp, "ibv_create_qp() failed");
  nvds_set_qp_state_init(ctx->qp);
}

static void nvds_run_server(nvds_context_t* ctx, nvds_ib_connection_t* remote_conn) {
  //nvds_server_exch_info(data);
  sleep(1); // Waitting for peer

  // Set queue pair state to RTR(Ready to receive)
  nvds_set_qp_state_rtr(ctx->qp, remote_conn);
  printf("server side: \n");
  //printf("\n");
  //nvds_print_qp_attr(ctx->qp);
  //printf("\n\n");

  /*
  printf("local connection: \n");
  nvds_print_ib_connection(&data->local_conn);
  printf("remote connection: \n");  
  nvds_print_ib_connection(&data->remote_conn);
  */
  // TODO(wgtdkp): poll request from client or do nothing
  while (1) {}
}

static void nvds_run_client(nvds_context_t* ctx, nvds_ib_connection_t* remote_conn) {
  sleep(2); // Waitting for peer

  nvds_set_qp_state_rts(ctx->qp, remote_conn);
  //nvds_set_qp_state_rtr(ctx->qp, data);
  printf("client side: \n");
  //printf("\n");
  //nvds_print_qp_attr(ctx->qp);
  //printf("\n\n");

  /*
  printf("local connection: \n");
  nvds_print_ib_connection(&data->local_conn);
  printf("remote connection: \n");  
  nvds_print_ib_connection(&data->remote_conn);
  */
  snprintf(ctx->buf, RDMA_WRITE_LEN, "hello rdma\n");

  static const int n = 1000 * 1000;  
  clock_t begin;
  double t;
  
  // Test write latency
  begin = clock();
  for (int i = 0; i < n; ++i) {
    // Step 1: RDMA write to server
    nvds_rdma_write(ctx, remote_conn);
    // Step 2: polling if RDMA write done
    nvds_poll_send(ctx);
  }
  t = (clock() - begin) * 1.0 / CLOCKS_PER_SEC;
  printf("time: %fs\n", t);
  printf("write latency: %fus\n", t / n * 1000 * 1000);

  abort();
  // Dump statistic info
}

Infiniband::QueuePairInfo qpis[2];

int nvds_main(int argc, const char* argv[]) {
  nvds_context_t ctx;

  nvds_expect(argc <= 2, "too many arguments");
  int is_client = 0;
  if (argc == 2) {
    is_client = 1;
  }

  page_size = sysconf(_SC_PAGESIZE);
  nvds_init_ctx(&ctx);
  if (is_client) {
    nvds_init_local_ib_connection(&ctx, &conn[0]);
    copy_conn_to_info(&qpis[0], &conn[0]);
    nvds_run_client(&ctx, &conn[1]);
  } else {
    nvds_init_local_ib_connection(&ctx, &conn[1]);
    copy_conn_to_info(&qpis[1], &conn[1]);
    nvds_run_server(&ctx, &conn[0]);
  }

  return 0;
}


static void RunClient() {
  try {
    Infiniband ib {1024};
    Infiniband::QueuePair qp(ib, IBV_QPT_RC, 128, 128);

    qpis[0] = {
      ib.GetLid(Infiniband::kPort),
      qp.GetLocalQPNum(),
      qp.psn,
      ib.mr()->rkey,
      reinterpret_cast<uint64_t>(ib.raw_mem())
    };
    copy_info_to_conn(&conn[0], &qpis[0]);
    
    sleep(2); // Waitting for peer
    printf("client side\n");
    qp.SetStateRTS(qpis[1]);
    assert(qp.GetState() == IBV_QPS_RTS);
    
    snprintf(ib.raw_mem(), RDMA_WRITE_LEN, "hello rdma\n");
    
    for (size_t i = 0; i < 10000; ++i) {
      ib.PostWriteAndWait(qp, qpis[1]);
    }

    std::cout << "finally, you bitch!" << std::endl;
  } catch (TransportException& e) {
    NVDS_ERR(e.ToString().c_str());
  }
}

static void RunServer() {
  try {
    Infiniband ib {1024};
    Infiniband::QueuePair qp(ib, IBV_QPT_RC, 128, 128);

    qpis[1] = {
      ib.GetLid(Infiniband::kPort),
      qp.GetLocalQPNum(),
      qp.psn,
      ib.mr()->rkey,
      reinterpret_cast<uint64_t>(ib.raw_mem())
    };
    copy_info_to_conn(&conn[1], &qpis[1]);
    
    sleep(1); // Waitting for peer
    printf("server side\n");
    qp.SetStateRTS(qpis[0]);

    assert(qp.GetState() == IBV_QPS_RTS);

    while (1) {}
  } catch (TransportException& e) {
    NVDS_ERR(e.ToString().c_str());
  }
}

///*
TEST (InfinibandTest, RDMAWrite) {
  const char* argv[] {"a.out", "192.168.99.14"};
  //thread server(bind(nvds_main, 1, argv));
  thread server(RunServer);
  //thread client(bind(nvds_main, 2, argv));
  thread client(RunClient);
  
  client.join();
  server.join();
}
//*/

/*
TEST (InfinibandTest, RDMAWrite) {
  thread server(RunServer);
  thread client(RunClient);
  client.join();
  server.join(); 
}
*/

/*
TEST (InfinibandTest, RDMAWrite) {
  try {
    Infiniband ib;
    char bufs[2][1024] {{0}};
    ibv_mr* mr = ibv_reg_mr(ib.pd().pd(), bufs, sizeof(bufs),
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    assert(mr != nullptr);

    auto scq0 = ib.CreateCQ(128);
    auto rcq0 = ib.CreateCQ(128);
    Infiniband::QueuePair qp0(ib, IBV_QPT_RC, 1,
        nullptr, scq0, rcq0, 128, 128);
    auto scq1 = ib.CreateCQ(128);
    auto rcq1 = ib.CreateCQ(128);
    Infiniband::QueuePair qp1(ib, IBV_QPT_RC, 1,
        nullptr, scq1, rcq1, 128, 128);

    Infiniband::QueuePairInfo qpi0 {
      ib.GetLid(1),
      qp0.GetLocalQPNum(),
      33,
      mr->rkey,
      reinterpret_cast<uint64_t>(bufs[0])
    };
    Infiniband::QueuePairInfo qpi1 {
      ib.GetLid(1),
      qp1.GetLocalQPNum(),
      33,
      mr->rkey,
      reinterpret_cast<uint64_t>(bufs[1])
    };
    
    qp0.Plumb(IBV_QPS_RTS, qpi1);
    qp1.Plumb(IBV_QPS_RTR, qpi0);

    struct ibv_sge sge {qpi0.vaddr, 64, mr->lkey};
    struct ibv_send_wr swr;
    swr.wr.rdma.remote_addr = qpi1.vaddr;
    swr.wr.rdma.rkey        = qpi1.rkey;
    swr.wr_id               = 1;
    swr.sg_list             = &sge;
    swr.num_sge             = 1;
    swr.opcode              = IBV_WR_RDMA_WRITE;
    swr.send_flags          = IBV_SEND_SIGNALED;
    swr.next                = nullptr;

    struct ibv_send_wr* bad_wr;
    int err = ibv_post_send(qp0.qp, &swr, &bad_wr);
    assert(err == 0);

    ibv_wc wc;
    int r;
    while ((r = ibv_poll_cq(qp0.scq, 1, &wc)) != 1) {}
    if (wc.status != IBV_WC_SUCCESS) {
      throw TransportException(HERE, wc.status);
    } else {
      std::clog << "success" << std::endl << std::flush;
    }
  } catch (TransportException& e) {
    NVDS_ERR(e.ToString().c_str());
  }
}
*/

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
