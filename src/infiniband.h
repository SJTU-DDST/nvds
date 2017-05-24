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
  static const uint32_t kMaxInlineData = 128;

  Infiniband(uint32_t registered_mem_size=1024);
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

  struct Address {
		int32_t ib_port;
		uint16_t lid;
		uint32_t qpn;

		std::string ToString() const;
    bool operator==(const Address& other) const {
      return ib_port == other.ib_port &&
             lid == other.lid &&
             qpn == other.qpn;
    }
    bool operator!=(const Address& other) const {
      return !(*this == other);
    }
	};

	struct Buffer {
    Buffer*   next;
		char* 		buf;				
		uint32_t 	size;		    // Buffer size
		uint32_t 	msg_len;    // message length
		ibv_mr*		mr;         // IBV memory region
		Address   peer_addr;  // Peer address of this request
		bool 			is_recv;    // Is this a recv buffer

		Buffer(char* b, uint32_t size, ibv_mr* mr, bool is_recv=false)
				: next(nullptr), buf(b), size(size), mr(mr), is_recv(is_recv) {}
		Buffer() : next(nullptr), buf(nullptr), size(0), msg_len(0),
               mr(nullptr), is_recv(false) {}
		DISALLOW_COPY_AND_ASSIGN(Buffer);
    Request* MakeRequest() {
      return reinterpret_cast<Request*>(buf + kIBUDPadding);
    }
    Response* MakeResponse() {
      // TODO(wgtdkp): are you sure ?
      return reinterpret_cast<Response*>(buf + kIBUDPadding);
    }
	};

  // A registered buffer pool
	class RegisteredBuffers {
	 public:
	  RegisteredBuffers(ibv_pd* pd, uint32_t buf_size,
                      uint32_t buf_num, bool is_recv=false);
		~RegisteredBuffers();
		DISALLOW_COPY_AND_ASSIGN(RegisteredBuffers);
    // Used as buffer pool
    Buffer* Alloc() {
      std::lock_guard<Spinlock> _(spinlock_);
      auto ret = root_;
      if (root_ != nullptr) {
        root_ = root_->next;
      }
      return ret;
    }
    void Free(Buffer* b) {
      std::lock_guard<Spinlock> _(spinlock_);
      b->next = root_;
      root_ = b;
    }
    const Buffer* root() const { return root_; }
	 private:
	 	uint32_t buf_size_;
		uint32_t buf_num_;
		void* ptr_;
		Buffer* bufs_;
    Buffer* root_;
    Spinlock spinlock_;
	};

  uint16_t GetLid(uint16_t port);
  Buffer* TryReceive(QueuePair* qp) { return PollCQ(qp->rcq, 1); }
  Buffer* TrySend(QueuePair* qp) { return PollCQ(qp->scq, 1); }
  Buffer* PollCQ(ibv_cq* cq, int num_entries);
  Buffer* Receive(QueuePair* qp);
	void PostReceive(QueuePair* qp, Buffer* b);
  void PostSend(QueuePair* qp, Buffer* b, uint32_t len,
								const Address* peer_addr);
  void PostSendAndWait(QueuePair* qp, Buffer* b, uint32_t len,
											 const Address* peer_addr);
  ibv_ah* GetAddrHandler(const Address& addr);

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

  std::array<ibv_ah*, UINT16_MAX + 1> addr_handlers_;
};

} // namespace nvds

#endif // _NVDS_INFINIBAND_H_
