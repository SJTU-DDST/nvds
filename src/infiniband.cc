#include "infiniband.h"

#include <algorithm>
#include <malloc.h>

namespace nvds {

Infiniband::Infiniband(uint32_t registered_mem_size)
    : ctx_(nullptr), pd_(nullptr), mr_(nullptr),
      raw_mem_(nullptr), raw_mem_size_(registered_mem_size) {
  struct ibv_device** dev_list = ibv_get_device_list(NULL);
  if (dev_list == nullptr) {
    throw TransportException(HERE, "get IB device list failed", errno);
  }
  ctx_ = ibv_open_device(dev_list[0]);
  if (ctx_ == nullptr) {
    throw TransportException(HERE, "open IB device 0 failed", errno);
  }
  pd_ = ibv_alloc_pd(ctx_);
  if (pd_ == nullptr) {
    throw TransportException(HERE, "alloc protection domain failed", errno);
  }
  raw_mem_ = reinterpret_cast<char*>(memalign(4096, raw_mem_size_));
  assert(raw_mem_ != nullptr);

  int access = IBV_ACCESS_LOCAL_WRITE |
               IBV_ACCESS_REMOTE_READ |
               IBV_ACCESS_REMOTE_WRITE;
  mr_ = ibv_reg_mr(pd_, raw_mem_, raw_mem_size_, access);
  if (mr_ == nullptr) {
    throw TransportException(HERE, "register memory failed", errno);
  }
}

Infiniband::~Infiniband() {
  // Release in reverse order
  int err;
  err = ibv_dereg_mr(mr_);
  assert(err == 0);
  free(raw_mem_);
  err = ibv_dealloc_pd(pd_);
  assert(err == 0);
  err = ibv_close_device(ctx_);
  assert(err == 0);
}

void Infiniband::QueuePairInfo::Print() const {
  std::cout << "lid: " << lid << std::endl;
  std::cout << "qpn: " << qpn << std::endl;
  std::cout << "psn: " << psn << std::endl;
  std::cout << "rkey: " << rkey << std::endl;
  std::cout << "vaddr: " << vaddr << std::endl;
}

Infiniband::QueuePair::QueuePair(Infiniband& ib, ibv_qp_type type,
                                 uint32_t max_send, uint32_t max_recv)
    : ib(ib), type(type), qp(nullptr),
      scq(nullptr), rcq(nullptr), psn(kDefaultPsn) {
  assert(type == IBV_QPT_RC || type == IBV_QPT_UD);

  scq = ibv_create_cq(ib.ctx(), max_send, nullptr, nullptr, 0);
  rcq = ibv_create_cq(ib.ctx(), max_recv, nullptr, nullptr, 0);
  assert(scq != nullptr && rcq != nullptr);

  ibv_qp_init_attr init_attr;
  init_attr.srq     = nullptr;
  init_attr.send_cq = scq;
  init_attr.recv_cq = rcq;
  init_attr.qp_type = type;
  init_attr.cap.max_send_wr = max_send;
  init_attr.cap.max_recv_wr = max_recv;
  init_attr.cap.max_send_sge = Infiniband::kMaxSendSge;
  init_attr.cap.max_recv_sge = Infiniband::kMaxRecvSge;
  init_attr.cap.max_inline_data = Infiniband::kMaxInlineData;

  qp = ibv_create_qp(ib.pd(), &init_attr);
  if (qp == nullptr) {
    throw TransportException(HERE, "create queue pair failed", errno);
  }

  ibv_qp_attr attr;
  attr.qp_state = IBV_QPS_INIT;
  attr.pkey_index = 0;
  attr.port_num = Infiniband::kPort;
  attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                         IBV_ACCESS_REMOTE_READ |
                         IBV_ACCESS_REMOTE_WRITE;
  int modify = IBV_QP_STATE | IBV_QP_PKEY_INDEX |
               IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
  int ret = ibv_modify_qp(qp, &attr, modify);
  if (ret != 0) {
    ibv_destroy_qp(qp);
    throw TransportException(HERE, ret);
  }
}

Infiniband::QueuePair::~QueuePair() {
  int err = ibv_destroy_qp(qp);
  assert(err == 0);
}

uint32_t Infiniband::QueuePair::GetPeerQPNum() const {
  ibv_qp_attr attr;
  ibv_qp_init_attr init_attr;
  auto err = ibv_query_qp(qp, &attr, IBV_QP_DEST_QPN, &init_attr);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
  return attr.dest_qp_num;
}

int Infiniband::QueuePair::GetState() const {
  ibv_qp_attr attr;
  ibv_qp_init_attr init_attr;
  int err = ibv_query_qp(qp, &attr, IBV_QP_STATE, &init_attr);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
  return attr.qp_state;
}

// Bring UD queue pair into RTS status
void Infiniband::QueuePair::Activate() {
  assert(type == IBV_QPT_UD);
  assert(GetState() == IBV_QPS_INIT);

  ibv_qp_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.qp_state = IBV_QPS_RTR;
  auto err = ibv_modify_qp(qp, &attr, IBV_QP_STATE);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
  
  attr.qp_state = IBV_QPS_RTS;
  attr.sq_psn = psn;
  err = ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_SQ_PSN);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
}

void Infiniband::QueuePair::SetStateRTR(const QueuePairInfo& peer_info) {
  assert(type == IBV_QPT_RC);
  assert(GetState() == IBV_QPS_INIT);

  struct ibv_qp_attr attr;
  // We must set qp into RTR state before RTS
  attr.qp_state           = IBV_QPS_RTR;
  attr.path_mtu           = IBV_MTU_4096;
  attr.dest_qp_num        = peer_info.qpn;
  attr.rq_psn             = peer_info.psn;
  attr.max_dest_rd_atomic = 1;
  attr.min_rnr_timer      = 12;
  attr.ah_attr.is_global  = 0;
  attr.ah_attr.dlid       = peer_info.lid;
  attr.ah_attr.sl         = 0;
  attr.ah_attr.src_path_bits = 0;
  attr.ah_attr.port_num      = Infiniband::kPort;

  int modify = IBV_QP_STATE               |
               IBV_QP_AV                  |
               IBV_QP_PATH_MTU            |
               IBV_QP_DEST_QPN            |
               IBV_QP_RQ_PSN              |
               IBV_QP_MAX_DEST_RD_ATOMIC  |
               IBV_QP_MIN_RNR_TIMER;

  int err = ibv_modify_qp(qp, &attr, modify);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
}

void Infiniband::QueuePair::SetStateRTS(const QueuePairInfo& peer_info) {
  SetStateRTR(peer_info);

  struct ibv_qp_attr attr;
  attr.qp_state   = IBV_QPS_RTS;
  attr.timeout    = 14;
  attr.retry_cnt  = 7;
  attr.rnr_retry  = 7;
  attr.sq_psn     = psn;
  attr.max_rd_atomic = 1;

  int modify = IBV_QP_STATE            |
               IBV_QP_TIMEOUT          |
               IBV_QP_RETRY_CNT        |
               IBV_QP_RNR_RETRY        |
               IBV_QP_SQ_PSN           |
               IBV_QP_MAX_QP_RD_ATOMIC;

  int err = ibv_modify_qp(qp, &attr, modify);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
}

Infiniband::RegisteredBuffers::RegisteredBuffers(ibv_pd* pd,
    uint32_t buf_size, uint32_t buf_num, bool is_recv)
    : buf_size_(buf_size), buf_num_(buf_num),
      ptr_(nullptr), bufs_(nullptr), root_(nullptr) {
  const size_t bytes = buf_size * buf_num;
  ptr_ = memalign(kPageSize, bytes);
  assert(ptr_ != nullptr);
  int access = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE;
  auto mr = ibv_reg_mr(pd, ptr_, bytes, access);
  if (mr == nullptr) {
    throw TransportException(HERE, "failed to register buffers", errno);
  }
  bufs_ = new Buffer[buf_num_];
  char* p = static_cast<char*>(ptr_);
  for (uint32_t i = 0; i < buf_num; ++i) {
    new (&bufs_[i]) Buffer(p, buf_size_, mr, is_recv);
    bufs_[i].next = i + 1 < buf_num ? &bufs_[i+1] : nullptr;
    p += buf_size_;
  }
  root_ = bufs_;
}

Infiniband::RegisteredBuffers::~RegisteredBuffers() {
  // Deregsiter memory region
  ibv_dereg_mr(bufs_[0].mr);
  free(ptr_);
  delete[] bufs_;
}

uint16_t Infiniband::GetLid(uint16_t port) {
  ibv_port_attr port_attr;
  auto ret = ibv_query_port(ctx_, port, &port_attr);
  if (ret != 0) {
    throw TransportException(HERE, ret);
  }
  return port_attr.lid;
}

Infiniband::Buffer* Infiniband::PollCQ(ibv_cq* cq, int num_entries) {
  ibv_wc wc;
  int r = ibv_poll_cq(cq, num_entries, &wc);
  if (r == 0) {
    return NULL;
  } else if (r < 0) {
    throw TransportException(HERE, r);
  } else if (wc.status != IBV_WC_SUCCESS) {
    throw TransportException(HERE, wc.status);
  }
  // wr_id is used as buffer address
  auto b = reinterpret_cast<Buffer*>(wc.wr_id);
  b->msg_len = wc.byte_len;
  // FIXME(wgtdkp):
  b->peer_addr = {Infiniband::kPort/*qp->ib_port*/, wc.slid, wc.src_qp};
  return b;
}

// May blocking
Infiniband::Buffer* Infiniband::Receive(QueuePair* qp) {
  Buffer* b;
  while ((b = TryReceive(qp)) == nullptr) {}
  return b;
}

void Infiniband::PostReceive(QueuePair* qp, Buffer* b) {
  ibv_sge sge {
    reinterpret_cast<uint64_t>(b->buf),
    b->size,
    b->mr->lkey
  };
  ibv_recv_wr rwr;
  memset(&rwr, 0, sizeof(rwr));
  rwr.wr_id = reinterpret_cast<uint64_t>(b);
  rwr.next = nullptr;
  rwr.sg_list = &sge;
  rwr.num_sge = 1;

  ibv_recv_wr* bad_rwr;
  auto err = ibv_post_recv(qp->qp, &rwr, &bad_rwr);
  if (err != 0) {
    throw TransportException(HERE, "ibv_post_recv failed", err);
  }
}

void Infiniband::PostSend(QueuePair* qp, Buffer* b,
    uint32_t len, const Address* peer_addr) {
  assert(qp->type == IBV_QPT_UD);

  ibv_sge sge {
    reinterpret_cast<uint64_t>(b->buf),
    len,
    b->mr->lkey
  };
  ibv_send_wr swr;
  memset(&swr, 0, sizeof(swr));
  swr.wr_id = reinterpret_cast<uint64_t>(b);
  if (qp->type == IBV_QPT_UD) {
    swr.wr.ud.ah = GetAddrHandler(*peer_addr);
    swr.wr.ud.remote_qpn = peer_addr->qpn;
    swr.wr.ud.remote_qkey = 0;
    assert(swr.wr.ud.ah != nullptr);
  }
  swr.next = nullptr;
  swr.sg_list = &sge;
  swr.num_sge = 1;
  swr.opcode = IBV_WR_SEND;
  swr.send_flags = IBV_SEND_SIGNALED;
  if (len <= kMaxInlineData) {
    swr.send_flags |= IBV_SEND_INLINE;
  }

  ibv_send_wr* bad_swr;
  auto err = ibv_post_send(qp->qp, &swr, &bad_swr);
  if (err != 0) {
    throw TransportException(HERE, "ibv_post_send failed", err);
  }
}

void Infiniband::PostSendAndWait(QueuePair* qp, Buffer* b,
    uint32_t len, const Address* peer_addr) {
  assert(qp->type == IBV_QPT_UD);

  PostSend(qp, b, len, peer_addr);

  ibv_wc wc;
  while (ibv_poll_cq(qp->scq, 1, &wc) < 1) {}
  // FIXME(wgtdkp):
  // How can we make sure that the completed work we polled is the one
  // we have just post?
  if (wc.status != IBV_WC_SUCCESS) {
    throw TransportException(HERE, "PostSendAndWait failed", wc.status);
  }
}

ibv_ah* Infiniband::GetAddrHandler(const Address& addr) {
  auto& ah = addr_handlers_[addr.lid];
  if (ah != nullptr) {
    return ah;
  }
  
  ibv_ah_attr attr;
  attr.is_global = 0;
  attr.dlid = addr.lid;
  attr.sl = 1;
  attr.src_path_bits = 0;
  attr.port_num = kPort;
  ah = ibv_create_ah(pd_, &attr);
  assert(ah != nullptr);
  return ah;
}

void Infiniband::PostWrite(QueuePair& qp, QueuePairInfo& peer_info) {
  struct ibv_sge sge;
  sge.addr   = reinterpret_cast<uint64_t>(raw_mem_);
  sge.length = 64;
  sge.lkey   = mr_->lkey;

  struct ibv_send_wr swr;
  swr.wr.rdma.remote_addr = peer_info.vaddr;
  swr.wr.rdma.rkey        = peer_info.rkey;
  swr.wr_id               = 3;
  swr.sg_list             = &sge;
  swr.num_sge             = 1;
  swr.opcode              = IBV_WR_RDMA_WRITE;
  swr.send_flags          = IBV_SEND_SIGNALED;
  swr.next                = nullptr;

  struct ibv_send_wr* bad_wr;
  int err = ibv_post_send(qp.qp, &swr, &bad_wr);
  if (err != 0) {
    throw TransportException(HERE, "post rdma write failed", err);
  }
}

void Infiniband::PostWriteAndWait(QueuePair& qp, QueuePairInfo& peer_info) {
  PostWrite(qp, peer_info);

  struct ibv_wc wc;
  int r;
  while ((r = ibv_poll_cq(qp.scq, 1, &wc)) != 1) {}
  if (wc.status != IBV_WC_SUCCESS) {
    throw TransportException(HERE, "poll completion queue failed", wc.status);
  }
}

} // namespace nvds
