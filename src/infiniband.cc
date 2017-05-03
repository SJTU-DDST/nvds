#include "infiniband.h"

#include <algorithm>
#include <malloc.h>

namespace nvds {

Infiniband::Infiniband(const char* device_name)
    : dev_(device_name), pd_(dev_) {
  std::fill(addr_handlers_.begin(), addr_handlers_.end(), nullptr);
}

Infiniband::~Infiniband() {
  for (auto iter = addr_handlers_.rbegin(); iter != addr_handlers_.rend(); ++iter) {
    auto ah = *iter;
    if (ah != nullptr)
      ibv_destroy_ah(ah);
  }
}

Infiniband::DeviceList::DeviceList()
    : dev_list_(ibv_get_device_list(nullptr)) {
  if (dev_list_ == nullptr) {
    throw TransportException(HERE, "Opening infiniband device failed", errno);
  }
}

ibv_device* Infiniband::DeviceList::Lookup(const char* name) {
  if (name == nullptr)
    return dev_list_[0];
  for (int i = 0; dev_list_[i] != nullptr; ++i) {
    if (strcmp(dev_list_[i]->name, name) == 0)
      return dev_list_[i];
  }
  return nullptr;
}

Infiniband::Device::Device(const char* name) {
  DeviceList dev_list;
  auto dev = dev_list.Lookup(name);
  if (dev == nullptr) {
    throw TransportException(HERE,
        Format("failed to find infiniband devide: %s", name ? name: "any"));
  }
  
  ctx_ = ibv_open_device(dev);
  if (ctx_ == nullptr) {
    throw TransportException(HERE, 
        Format("failed to find infiniband devide: %s", name ? name: "any"),
        errno);
  }
}

Infiniband::QueuePair::QueuePair(Infiniband& ib, ibv_qp_type type,
    int ib_port, ibv_srq* srq, ibv_cq* scq, ibv_cq* rcq,
    uint32_t max_send, uint32_t max_recv)
    : ib(ib), type(type), ctx(ib.dev().ctx()),
      ib_port(ib_port), pd(ib.pd().pd()), srq(srq),
      qp(nullptr), scq(scq), rcq(rcq), psn(0), peer_lid(0), sin() {
  assert(type == IBV_QPT_UD || type == IBV_QPT_RC);

  ibv_qp_init_attr qpia;
  memset(&qpia, 0, sizeof(qpia));
  qpia.send_cq = scq;
  qpia.recv_cq = rcq;
  qpia.srq = srq;
  qpia.cap.max_send_wr = max_send;
  qpia.cap.max_recv_wr = max_recv;
  // FIXME(wgtdkp): what is the `max_send_sge` for replication `qp`
  qpia.cap.max_send_sge = type == IBV_QPT_UD ? 1 : 8;
  qpia.cap.max_recv_sge = type == IBV_QPT_UD ? 1 : 8;
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
  qpa.qkey = kQkey;

  int mask = IBV_QP_STATE | IBV_QP_PORT;
  switch (type) {
  case IBV_QPT_RC:
    mask |= IBV_QP_ACCESS_FLAGS;
    mask |= IBV_QP_PKEY_INDEX;
    break;
  case IBV_QPT_UD:
    mask |= IBV_QP_QKEY;
    mask |= IBV_QP_PKEY_INDEX;
    break;
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

// Called only on RC queue pair
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

  auto err = ibv_modify_qp(qp, &qpa,
      IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
      IBV_QP_RQ_PSN | IBV_QP_MIN_RNR_TIMER | IBV_QP_MAX_DEST_RD_ATOMIC);
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

  err = ibv_modify_qp(qp, &qpa,
      IBV_QP_STATE |  IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
      IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
  if (err != 0) {
    throw TransportException(HERE, err);
  }
  peer_lid = qpi->lid;
}

// Bring UD queue pair into RTS status
void Infiniband::QueuePair::Activate() {
  assert(type == IBV_QPT_UD);
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

Infiniband::RegisteredBuffers::RegisteredBuffers(ProtectionDomain& pd,
    uint32_t buf_size, uint32_t buf_num, bool is_recv)
    : buf_size_(buf_size), buf_num_(buf_num),
      ptr_(nullptr), bufs_(nullptr), root_(nullptr) {
  const size_t bytes = buf_size * buf_num;
  ptr_ = memalign(kPageSize, bytes);
  assert(ptr_ != nullptr);
  auto mr = ibv_reg_mr(pd.pd(), ptr_, bytes,
                       IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
  if (mr == nullptr) {
    throw TransportException(HERE, "failed to register buffer", errno);
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

/*
Infiniband::QueuePair* Infiniband::CreateQP(ibv_qp_type type, int ib_port,
    ibv_srq* srq, ibv_cq* scq, ibv_cq* rcq, uint32_t max_send,
    uint32_t max_recv, uint32_t qkey) {
  return new QueuePair(*this, type, ib_port, srq, scq, rcq,
                       max_send, max_recv, qkey);
}
*/
uint16_t Infiniband::GetLid(int32_t port) {
  ibv_port_attr port_attr;
  auto ret = ibv_query_port(dev_.ctx(), static_cast<uint8_t>(port), &port_attr);
  if (ret != 0) {
    throw TransportException(HERE, ret);
  }
  return port_attr.lid;
}

Infiniband::Buffer* Infiniband::TryReceive(QueuePair* qp) {
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
  b->peer_addr = {qp->ib_port, wc.slid, wc.src_qp};
  return b;
}

// May blocking
Infiniband::Buffer* Infiniband::Receive(QueuePair* qp) {
  Buffer* b = nullptr;
  do {
    b = TryReceive(qp);
  } while (b == nullptr);
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

void Infiniband::PostSRQReceive(ibv_srq* srq, Buffer* b) {
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
  auto err = ibv_post_srq_recv(srq, &rwr, &bad_rwr);
  if (err != 0) {
    throw TransportException(HERE, "ibv_post_srq_recv failed", err);
  }
}

void Infiniband::PostSend(QueuePair* qp, Buffer* b,
    uint32_t len, const Address* peer_addr, uint32_t peer_qkey) {
  if (qp->type == IBV_QPT_UD) {
      assert(peer_addr != nullptr);
  } else {
      assert(peer_addr == nullptr);
      assert(peer_qkey == 0);
  }

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
    swr.wr.ud.remote_qkey = peer_qkey;
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
    uint32_t len, const Address* peer_addr, uint32_t peer_qkey) {
  assert(qp->type == IBV_QPT_UD);

  PostSend(qp, b, len, peer_addr, peer_qkey);

  ibv_wc wc;
  while (ibv_poll_cq(qp->scq, 1, &wc) < 1) {}
  // FIXME(wgtdkp):
  // How can we make sure that the completion element we polled is the one
  // we have just sent?
  if (wc.status != IBV_WC_SUCCESS) {
    throw TransportException(HERE, "PostSendAndWiat failed", wc.status);
  }
}

ibv_ah* Infiniband::GetAddrHandler(const Address& addr) {
  auto& ah = addr_handlers_[addr.lid];
  if (ah != nullptr) {
    return ah;
  }
  
  ibv_ah_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.is_global = 0;
  attr.dlid = addr.lid;
  attr.sl = 1;
  attr.src_path_bits = 0;
  attr.port_num = kPort;
  return ah = ibv_create_ah(pd_.pd(), &attr);
}

} // namespace nvds
