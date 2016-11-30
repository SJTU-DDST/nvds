#include "infiniband.h"

static void nvds_run_server(nvds_ib_context_t* ctx, nvds_ib_data_t* data);
static void nvds_run_client(nvds_ib_context_t* ctx, nvds_ib_data_t* data);

static void nvds_run_server(nvds_ib_context_t* ctx, nvds_ib_data_t* data) {
  nvds_ib_server_exch_info(data);

  // Set queue pair state to RTR(Read to Receive)
  nvds_ib_set_qp_state_rtr(ctx->qp, data);

  // TODO(wgtdkp): poll request from client or do nothing
  while (1) {}
}

/*
static void nvds_multi_rdma_write(nvds_ib_context_t* ctx, nvds_ib_data_t* data) {

}

static void nvds_test_multi_sge(nvds_ib_context_t* ctx, nvds_ib_data_t* data) {
  static const int n = 1000 * 1000;


}
*/

static void nvds_run_client(nvds_ib_context_t* ctx, nvds_ib_data_t* data) {
  nvds_ib_client_exch_info(data);
  nvds_ib_set_qp_state_rts(ctx->qp, data);

  snprintf(ctx->buf, RDMA_WRITE_LEN, "hello rdma\n");

  static const int n = 1000 * 1000;  
  clock_t begin;
  double t;
  
  // Test write latency
  begin = clock();
  for (int i = 0; i < n; ++i) {
    // Step 1: RDMA write to server
    nvds_ib_rdma_write(ctx, data);
    // Step 2: polling if RDMA write done
    nvds_ib_poll_send(ctx);
  }
  t = (clock() - begin) * 1.0 / CLOCKS_PER_SEC;
  printf("time: %fs\n", t);
  printf("write latency: %fus\n", t / n * 1000 * 1000);

  // Test read latency
  begin = clock();
  for (int i = 0; i < n; ++i) {
    // Step 1: RDMA read from server
    nvds_ib_rdma_read(ctx, data);
    // Step 2: polling if RDMA read done
    nvds_ib_poll_send(ctx);
  }
  t = (clock() - begin) * 1.0 / CLOCKS_PER_SEC;
  printf("time: %fs\n", t);
  printf("read latency: %fus\n", t / n * 1000 * 1000);

  /*
  for (int i = 0; i < n; ++i) {
    // Step 1: RDMA write to server
    snprintf(ctx->buf, RDMA_WRITE_LEN, "hello rdma\n");
    nvds_ib_rdma_write(ctx, data);
    // Step 2: polling if RDMA write done
    nvds_ib_poll_send(ctx);
    // Step 3: RDMA read from server
    nvds_ib_rdma_read(ctx, data);
    // Step 4: polling if RDMA read done
    nvds_ib_poll_send(ctx);
    // Step 5: verify read data
    //char* write_buf = ctx->buf;
    //char* read_buf = ctx->buf + RDMA_WRITE_LEN;
    //nvds_expect(strncmp(write_buf, read_buf, RDMA_WRITE_LEN) == 0,
    //            "data read dirty");
    //printf("%s", read_buf);
    //printf("%d th write/read completed\n", i);
    //memset(ctx->buf, 0, ctx->size);
  }

  double t = (clock() - begin) * 1.0 / CLOCKS_PER_SEC;
  printf("time: %fs\n", t);
  printf("QPS: %f\n", n / t);
  printf("client exited\n");
  */
  exit(0);
  // Dump statistic info
}

int main(int argc, const char* argv[]) {
  nvds_ib_context_t ctx;
  nvds_ib_data_t data = {
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

  // Init
  pid_t pid = getpid();
  srand48(pid * time(NULL));
  page_size = sysconf(_SC_PAGESIZE);
  nvds_ib_init_ctx(&ctx, &data);
  nvds_ib_init_local_connection(&ctx, &data);

  if (is_client) {
    nvds_run_client(&ctx, &data);
  } else {
    nvds_run_server(&ctx, &data);
  }

  return 0;
}
