#include "infiniband.h"

#include <malloc.h>

namespace nvds {

Infiniband::Infiniband(const char* device_name)
    : dev_(device_name), pd_(dev_) {
}

Infiniband::Device::Device(const char* name) {
  DeviceList dev_list;
  auto dev = dev_list.Lookup(name);
  if (dev == nullptr) {
    throw TransportException(HERE,
                             Format("failed to find infiniband devide: %s",
                                    name ? name: "any"));
  }
  
  ctx_ = ibv_open_device(dev);
  if (ctx_ == nullptr) {
    throw TransportException(HERE, 
                             Format("failed to find infiniband devide: %s",
                                    name ? name: "any"), errno);
  }
}

Infiniband::QueuePair::QueuePair(Infiniband& ib, ibv_qp_type type,
                                 int ib_port, ibv_srq* srq, ibv_cq* scq,
							                   ibv_cq* rcq, uint32_t max_send,
							                   uint32_t max_recv, uint32_t qkey)
    : ib(ib), type(type), ctx(ib.dev().ctx()),
      ib_port(ib_port), pd(ib.pd().pd()), srq(srq),
      qp(nullptr), scq(scq), rcq(rcq), psn(0), peer_lid(0), sin() {
  assert(type == IBV_QPT_RC || type == IBV_QPT_UD);

  ibv_qp_init_attr qpia;
  memset(&qpia, 0, sizeof(qpia));
  qpia.send_cq = scq;
  qpia.recv_cq = rcq;
  qpia.srq = srq;
  qpia.cap.max_send_wr = max_send;
  qpia.cap.max_recv_wr = max_recv;
  qpia.cap.max_inline_data = kMaxInlineData;
  qpia.qp_type = type;
  qpia.sq_sig_all = 0; // Only generate CQEs on requested WQEs

  qp = ibv_create_qp(pd, &qpia);
  if (qp == nullptr) {
    throw TransportException(HERE, "failed to create queue pair");
  }

  // Set queue pair to status:INIT
  ibv_qp_attr qpa;
  memset(&qpa, 0, sizeof(qpa));
  qpa.qp_state = IBV_QPS_INIT;
  qpa.pkey_index = 0;
  qpa.port_num = static_cast<uint8_t>(ib_port);
  qpa.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE;
  qpa.qkey = qkey;

  int mask = IBV_QP_STATE | IBV_QP_PORT;
  switch (type) {
  case IBV_QPT_RC:
    mask |= IBV_QP_ACCESS_FLAGS;
    mask |= IBV_QP_PKEY_INDEX;
    break;
  //case IBV_QPT_UD:
  //  mask |= IBV_QP_QKEY;
  //  mask |= IBV_QP_PKEY_INDEX;
  //  break;
  default:
    assert(false);
  }

  int ret = ibv_modify_qp(qp, &qpa, mask);
  if (ret != 0) {
    ibv_destroy_qp(qp);
    throw TransportException(HERE, ret);
  }
}

uint32_t Infiniband::QueuePair::GetPeerQPNum() const {
  ibv_qp_attr qpa;
  ibv_qp_init_attr qpia;

  auto err = ibv_query_qp(qp, &qpa, IBV_QP_DEST_QPN, &qpia);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
  return qpa.dest_qp_num;
}

int Infiniband::QueuePair::GetState() const {
  ibv_qp_attr qpa;
  ibv_qp_init_attr qpia;

  int err = ibv_query_qp(qp, &qpa, IBV_QP_STATE, &qpia);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
  return qpa.qp_state;
}

/*
 * Bring a queue pair into RTS state
 */
void Infiniband::QueuePair::Plumb(QueuePairInfo* qpi) {
  assert(type == IBV_QPT_RC);
  assert(GetState() == IBV_QPS_INIT);

  ibv_qp_attr qpa;
  memset(&qpa, 0, sizeof(qpa));
  // We must set qp into RTR state before RTS
  qpa.qp_state = IBV_QPS_RTR;
  qpa.path_mtu = IBV_MTU_1024;
  qpa.dest_qp_num = qpi->qpn;
  qpa.rq_psn = qpi->psn;
  qpa.max_dest_rd_atomic = 1;
  qpa.min_rnr_timer = 12;
  qpa.ah_attr.is_global = 0;
  qpa.ah_attr.dlid = qpi->lid;
  qpa.ah_attr.sl = 0;
  qpa.ah_attr.src_path_bits = 0;
  qpa.ah_attr.port_num = static_cast<uint8_t>(ib_port);

  auto err = ibv_modify_qp(qp, &qpa, IBV_QP_STATE |
                                    IBV_QP_AV |
                                    IBV_QP_PATH_MTU |
                                    IBV_QP_DEST_QPN |
                                    IBV_QP_RQ_PSN |
                                    IBV_QP_MIN_RNR_TIMER |
                                    IBV_QP_MAX_DEST_RD_ATOMIC);
  if (err != 0) {
    throw TransportException(HERE, err);
  }

  qpa.qp_state = IBV_QPS_RTS;

  // How long to wait before retrying if packet lost or server dead.
  // Supposedly the timeout is 4.096us*2^timeout.  However, the actual
  // timeout appears to be 4.096us*2^(timeout+1), so the setting
  // below creates a 135ms timeout.
  qpa.timeout = 14;

  // How many times to retry after timeouts before giving up.
  qpa.retry_cnt = 7;

  // How many times to retry after RNR (receiver not ready) condition
  // before giving up. Occurs when the remote side has not yet posted
  // a receive request.
  qpa.rnr_retry = 7; // 7 is infinite retry.
  qpa.sq_psn = psn;
  qpa.max_rd_atomic = 1;

  err = ibv_modify_qp(qp, &qpa, IBV_QP_STATE |
                                IBV_QP_TIMEOUT |
                                IBV_QP_RETRY_CNT |
                                IBV_QP_RNR_RETRY |
                                IBV_QP_SQ_PSN |
                                IBV_QP_MAX_QP_RD_ATOMIC);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
  peer_lid = qpi->lid;
}

void Infiniband::QueuePair::Activate() {
  assert(type == IBV_QPT_RC);
  assert(GetState() == IBV_QPS_INIT);

  ibv_qp_attr qpa;
  memset(&qpa, 0, sizeof(qpa));
  qpa.qp_state = IBV_QPS_RTR;
  auto err = ibv_modify_qp(qp, &qpa, IBV_QP_STATE);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
  
  qpa.qp_state = IBV_QPS_RTS;
  qpa.sq_psn = psn;
  err = ibv_modify_qp(qp, &qpa, IBV_QP_STATE | IBV_QP_SQ_PSN);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
}

std::string Infiniband::Address::ToString() const {
  return Format("%u:%u", lid, qpn);
}

Infiniband::RegisteredBuffers::RegisteredBuffers(
    ProtectionDomain& pd, uint32_t buf_size, uint32_t buf_num)
    : buf_size_(buf_size), buf_num_(buf_num), ptr_(nullptr), bufs_(nullptr) {
  const size_t bytes = buf_size * buf_num;
  ptr_ = memalign(4096, bytes);
  auto mr = ibv_reg_mr(pd.pd(), ptr_, bytes,
                       IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
  if (mr == nullptr) {
    throw TransportException(HERE, "failed to register buffer", errno);
  }
  bufs_ = new Buffer[buf_num_];
  char* p = static_cast<char*>(ptr_);
  for (uint32_t i = 0; i < buf_num; ++i) {
    new(&bufs_[i]) Buffer(p, buf_size_, mr);
    p += buf_size_;
  }
}

Infiniband::QueuePair* Infiniband::CreateQP(ibv_qp_type type,
                                            int ib_port,
											                      ibv_srq* srq,
                                            ibv_cq* scq,
											                      ibv_cq* rcq,
                                            uint32_t max_send,
											                      uint32_t max_recv,
                                            uint32_t qkey) {
  return new QueuePair(*this, type, ib_port, srq, scq, rcq,
                       max_send, max_recv, qkey);
}

int Infiniband::GetLid(int port) {
  ibv_port_attr port_attr;
  int ret = ibv_query_port(dev_.ctx(), static_cast<uint8_t>(port), &port_attr);
  if (ret != 0) {
    throw TransportException(HERE, ret);
  }
  return port_attr.lid;
}

Infiniband::Buffer* Infiniband::TryReceive(QueuePair* qp, Address* peer_addr) {
  ibv_wc wc;
  int r = ibv_poll_cq(qp->rcq, 1, &wc);
  if (r == 0) {
    return NULL;
  } else if (r < 0) {
    throw TransportException(HERE, r);
  } else if (wc.status != IBV_WC_SUCCESS) {
    throw TransportException(HERE, wc.status);
  }

  // wr_id is used as buffer address
  Buffer* b = reinterpret_cast<Buffer*>(wc.wr_id);
  b->msg_len = wc.byte_len;
  if (peer_addr != nullptr) {
    *peer_addr = {qp->ib_port, wc.slid, wc.src_qp};
  }
  return b;
}

} // namespace nvds

/*
#define RDMA_WRITE_ID   (3)
#define RDMA_READ_ID    (4)
#define RDMA_WRITE_LEN  (64)
#define RDMA_READ_LEN   RDMA_WRITE_LEN

namespace nvds {

static int page_size;

static void nvds_ib_init_local_connection(nvds_ib_context_t* ctx,
                                          nvds_ib_data_t* data);
static void nvds_ib_set_qp_state_init(struct ibv_qp* qp, nvds_ib_data_t* data);
static void nvds_ib_set_qp_state_rtr(struct ibv_qp* qp, nvds_ib_data_t* data);
static void nvds_ib_set_qp_state_rts(struct ibv_qp* qp, nvds_ib_data_t* data);

static void nvds_ib_set_qp_state_init(struct ibv_qp* qp, nvds_ib_data_t* data) {
  struct ibv_qp_attr attr = {
    qp_state: IBV_QPS_INIT,
    pkey_index: 0,
    port_num: data->ib_port,
    qp_access_flags: IBV_ACCESS_LOCAL_WRITE |
                     IBV_ACCESS_REMOTE_READ |
                     IBV_ACCESS_REMOTE_WRITE
  };

  int modify_flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                     IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
  nvds_expect(ibv_modify_qp(qp, &attr, modify_flags) == 0,
              "ibv_modify_qp() failed");
}

static void nvds_ib_set_qp_state_rtr(struct ibv_qp* qp, nvds_ib_data_t* data) {
  struct ibv_qp_attr attr = {
    qp_state:           IBV_QPS_RTR,
    path_mtu:           IBV_MTU_2048,
    dest_qp_num:        data->remote_conn.qpn,
    rq_psn:             data->remote_conn.psn,
    max_dest_rd_atomic: 1,
    min_rnr_timer:      12,
    ah_attr:            {
      is_global:      0,
      dlid:           data->remote_conn.lid,
      sl:             1,
      src_path_bits:  0,
      port_num:       data->ib_port
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

static void nvds_ib_set_qp_state_rts(struct ibv_qp* qp, nvds_ib_data_t* data) {
	// first the qp state has to be changed to rtr
	nvds_ib_set_qp_state_rtr(qp, data);
	
	struct ibv_qp_attr attr = {
	  qp_state:       IBV_QPS_RTS,
    timeout:        14,
    retry_cnt:      7,
    rnr_retry:      7,
    sq_psn:         data->local_conn.psn,
    max_rd_atomic:  1
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

void nvds_ib_server_exch_info(nvds_ib_data_t* data) {
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

void nvds_ib_client_exch_info(nvds_ib_data_t* data) {
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
  printf("information exchanged\n");
}

void nvds_ib_rdma_write(nvds_ib_context_t* ctx, nvds_ib_data_t* data) {
  ctx->sge.addr    = (uintptr_t)ctx->buf;
  ctx->sge.length  = RDMA_WRITE_LEN;
  ctx->sge.lkey    = ctx->mr->lkey;

  // The address write to
  ctx->wr.wr.rdma.remote_addr = data->remote_conn.vaddr;
  ctx->wr.wr.rdma.rkey        = data->remote_conn.rkey;
  ctx->wr.wr_id               = RDMA_WRITE_ID;
  ctx->wr.sg_list             = &ctx->sge;
  ctx->wr.num_sge             = 1;
  ctx->wr.opcode              = IBV_WR_RDMA_WRITE;
  ctx->wr.send_flags          = IBV_SEND_SIGNALED;
  ctx->wr.next                = NULL;

  struct ibv_send_wr* bad_wr;
  nvds_expect(ibv_post_send(ctx->qp, &ctx->wr, &bad_wr) == 0,
              "ibv_post_send() failed");
}

void nvds_ib_rdma_read(nvds_ib_context_t* ctx, nvds_ib_data_t* data) {
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

void nvds_ib_poll_send(nvds_ib_context_t* ctx) {
  struct ibv_wc wc;
  while (ibv_poll_cq(ctx->scq, 1, &wc) != 1) {}
  nvds_expect(wc.status == IBV_WC_SUCCESS, "rdma write failed");
  nvds_expect(wc.wr_id == ctx->wr.wr_id, "wr id not matched");
}

void nvds_ib_poll_recv(nvds_ib_context_t* ctx) {
  struct ibv_wc wc;
  while (ibv_poll_cq(ctx->scq, 1, &wc) != 1) {}
  nvds_expect(wc.status == IBV_WC_SUCCESS, "rdma read failed");
  nvds_expect(wc.wr_id == ctx->wr.wr_id, "wr id not matched");
}

static void nvds_ib_init_local_connection(nvds_ib_context_t* ctx,
                                          nvds_ib_data_t* data) {
  struct ibv_port_attr attr;
  nvds_expect(ibv_query_port(ctx->context, data->ib_port, &attr) == 0,
              "ibv_query_port() failed");
  data->local_conn.lid = attr.lid;
  data->local_conn.qpn = ctx->qp->qp_num;
  data->local_conn.psn = lrand48() & 0xffffff;
  data->local_conn.rkey = ctx->mr->rkey;
  data->local_conn.vaddr = (uintptr_t)ctx->buf;
}

void nvds_ib_init_ctx(nvds_ib_context_t* ctx, nvds_ib_data_t* data) {
  memset(ctx, 0, sizeof(nvds_ib_context_t));
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

  // Create & init queue pair
  struct ibv_qp_init_attr qp_init_attr = {
    send_cq: ctx->scq,
    recv_cq: ctx->rcq,
    qp_type: IBV_QPT_RC,
    cap: {
      max_send_wr:      ctx->tx_depth,
      max_recv_wr:      1,
      max_send_sge:     1,
      max_recv_sge:     1,
      max_inline_data:  0
    }
  };
  ctx->qp = ibv_create_qp(ctx->pd, &qp_init_attr);
  nvds_expect(ctx->qp, "ibv_create_qp() failed");
  nvds_ib_set_qp_state_init(ctx->qp, data);
}

void nvds_ib_init() {
  // TODO(wgtdkp):

}

} // namespace nvds
*/
