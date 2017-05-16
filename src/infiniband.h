#ifndef _NVDS_INFINIBAND_H_
#define _NVDS_INFINIBAND_H_

#include "common.h"
#include "spinlock.h"
#include "transport.h"

#include <cstring>
#include <mutex>
#include <infiniband/verbs.h>
#include <netinet/ip.h>

namespace nvds {

struct Request;
struct Response;

// Derived from RAMCloud Infiniband.h
class Infiniband {
 public:
  static const uint16_t kPort = 1;
  static const uint32_t kMaxSendSge = 12;
  static const uint32_t kMaxRecvSge = 12;
  static const uint32_t kMaxInlineData = 64;

  Infiniband(uint32_t size);
  ~Infiniband();

  // This class includes informations for constructing a connection
	struct QueuePairInfo {
    uint16_t lid;   // Local id
		uint32_t qpn;   // Queue pair number
		uint32_t psn;   // Packet sequence number
		uint32_t rkey;  // Memory region key
    uint64_t vaddr; // Virtual address of memory regions
    // DEBUG
    void Print() const;
	};

  struct QueuePair {
    static const uint32_t kDefaultPsn = 33;
    Infiniband&   ib;
    ibv_qp_type   type;
    ibv_pd*       pd;
    ibv_srq*      srq;
    ibv_qp*       qp;
    ibv_cq*       scq;
    ibv_cq*       rcq;
    uint32_t      psn;

    QueuePair(Infiniband& ib, ibv_qp_type type,
              ibv_srq* srq, ibv_cq* scq, ibv_cq* rcq, 
              uint32_t max_send, uint32_t max_recv);
    ~QueuePair();
    DISALLOW_COPY_AND_ASSIGN(QueuePair);
    uint32_t GetLocalQPNum() const { return qp->qp_num; }
    uint32_t GetPeerQPNum() const;
    int GetState() const;
    void Activate();
    void SetStateRTR(const QueuePairInfo& peer_info);
    void SetStateRTS(const QueuePairInfo& peer_info);
  };

  uint16_t GetLid(uint16_t port);
  void PostWrite(QueuePair& qp, QueuePairInfo& peer_info);
  void PostWriteAndWait(QueuePair& qp, QueuePairInfo& peer_info);

  ibv_context* ctx() { return ctx_; }
  ibv_pd* pd() { return pd_; }
  ibv_mr* mr() { return mr_; }
  char* raw_mem() { return raw_mem_; }
  uint32_t raw_mem_size() { return raw_mem_size_; }

 private:
  ibv_context* ctx_;
  ibv_pd* pd_;
  ibv_mr* mr_;
  char* raw_mem_;
  uint32_t raw_mem_size_;
};

} // namespace nvds

#endif // _NVDS_INFINIBAND_H_
