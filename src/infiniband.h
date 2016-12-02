#ifndef _NVDS_INFINIBAND_H_
#define _NVDS_INFINIBAND_H_

#include "common.h"
#include "transport.h"

#include <cstring>
#include <infiniband/verbs.h>
#include <netinet/ip.h>

namespace nvds {

/*
typedef struct nvds_ib_context {
  struct ibv_context*       context;
	struct ibv_pd*            pd;
	struct ibv_mr*            mr;
	struct ibv_cq*            rcq;
	struct ibv_cq*            scq;
	struct ibv_qp*            qp;
	struct ibv_comp_channel*  ch;
	void*                     buf;
	unsigned            	    size;
	int                 	    tx_depth;
	struct ibv_sge      	    sge;
	struct ibv_send_wr  	    wr;
} nvds_ib_context_t;

typedef struct nvds_ib_connection {
  int             	  lid;
  int            	    qpn;
  int             	  psn;
	unsigned 			      rkey;
	unsigned long long 	vaddr;
} nvds_ib_connection_t;

typedef struct nvds_ib_data {
	int									  port;
	int									  ib_port;
	unsigned           	  size;
	int                		tx_depth;
	int 		    					sockfd;
	const char*           server_name;
	nvds_ib_connection_t	local_conn;
	nvds_ib_connection_t  remote_conn;
	struct ibv_device*    ib_dev;
} nvds_ib_data_t;

void nvds_ib_init();
void nvds_ib_rdma_write(nvds_ib_context_t* ctx, nvds_ib_data_t* data);
void nvds_ib_rdma_read(nvds_ib_context_t* ctx, nvds_ib_data_t* data);
void nvds_ib_poll_send(nvds_ib_context_t* ctx);
void nvds_ib_poll_recv(nvds_ib_context_t* ctx);
void nvds_ib_server_exch_info(nvds_ib_data_t* data);
void nvds_ib_client_exch_info(nvds_ib_data_t* data);
void nvds_ib_init_ctx(nvds_ib_context_t* ctx, nvds_ib_data_t* data);
*/

// Derived from RAMCloud Infiniband.h
class Infiniband {
 public:
 	static const uint32_t kMaxInlineData = 400;
	// If the device name is not specified,
	// choose the first one in device list
	explicit Infiniband(const char* device_name=nullptr);
	~Infiniband() {}

	// This class includes informations for constructing a connection
	struct QueuePairInfo {
		uint32_t qpn; // Queue pair number
		uint32_t psn; // Packet sequence number
		uint16_t lid; // Local id
		uint64_t nonce;
	};

	class DeviceList {
	 public:
		DeviceList(): dev_list_(nullptr) {}
		~DeviceList() { }
		DISALLOW_COPY_AND_ASSIGN(DeviceList);

		ibv_device* Lookup(const char* name) {
			if (dev_list_ == nullptr)
				return nullptr;
			if (name == nullptr)
				return dev_list_[0];
			for (int i = 0; dev_list_[i] != nullptr; ++i) {
				if (strcmp(dev_list_[i]->name, name) == 0)
					return dev_list_[i];
			}
			return nullptr;
		}

	 private:
		ibv_device** const dev_list_;
	};

	class Device {
	 public:
	 	explicit Device(const char* name=nullptr);
		~Device() { ibv_close_device(ctx_); }
		DISALLOW_COPY_AND_ASSIGN(Device);
		
		ibv_context* ctx() { return ctx_; }
		const ibv_context* ctx() const { return ctx_; }

	 private:
	 	ibv_context* ctx_;
	};

	class ProtectionDomain {
	 public:
	 	explicit ProtectionDomain(Device& device)
		 		: pd_(ibv_alloc_pd(device.ctx())) {
		  if (pd_ == nullptr) {
				throw TransportException(
					HERE,
					"failed to allocate infiniband protection domain",
					errno);
				
			}
		}
		~ProtectionDomain() { ibv_dealloc_pd(pd_); }
		DISALLOW_COPY_AND_ASSIGN(ProtectionDomain);
		operator ibv_pd() { return *pd_; }
		ibv_pd* pd() { return pd_; }
		const ibv_pd* pd() const { return pd_; }

	 private:
	 	ibv_pd* const pd_;
	};

	struct QueuePair {
		Infiniband& 	ib; // Infiniband this QP belongs to
		ibv_qp_type 	type;			 // QP type
		ibv_context* 	ctx;				 // Device context
		int 					ib_port;		 // Physical port
		ibv_pd* 			pd; 
		ibv_srq* 			srq;
		ibv_qp* 			qp;
		ibv_cq* 			scq;
		ibv_cq*				rcq;
		uint32_t 			psn;
		uint16_t 	    peer_lid;
		sockaddr_in 	sin;

	 	QueuePair(Infiniband& ib, ibv_qp_type type,
		 					int ib_port, ibv_srq* srq, ibv_cq* scq,
							ibv_cq* rcq, uint32_t max_send,
							uint32_t max_recv, uint32_t qkey=0);
		~QueuePair() { ibv_destroy_qp(qp); }
		DISALLOW_COPY_AND_ASSIGN(QueuePair);
		uint32_t GetLocalQPNum() const { return qp->qp_num; }
		uint32_t GetPeerQPNum() const;
		int GetState() const;
		void Plumb(QueuePairInfo* qpi);
		void Activate();
	};

	struct Address {
	 	//Infiniband& ib_;
		int ib_port;
		uint16_t lid;
		uint32_t qpn;

		std::string ToString() const;
	};

	struct Buffer {
		char* 		buf;				
		uint32_t 	size;				 // Buffer size
		uint32_t 	msg_len;     // message length
		ibv_mr*		mr;					 // IBV memory region
		uint16_t  peer_lid;    // Peer lid
		bool 			is_response; // Is this a response buffer

		Buffer(char* b, uint32_t size, ibv_mr* mr)
				: buf(b), size(size), mr(mr) {}
		Buffer()
				: buf(nullptr), size(0), msg_len(0), mr(nullptr),
				  peer_lid(0), is_response(false) {}
		DISALLOW_COPY_AND_ASSIGN(Buffer);
	};

	class RegisteredBuffers {
	 public:
	  RegisteredBuffers(ProtectionDomain& pd, 
											uint32_t buf_size, uint32_t buf_num);
		~RegisteredBuffers() { free(ptr_); }
		DISALLOW_COPY_AND_ASSIGN(RegisteredBuffers);

		Buffer& GetBuffer(const void* pos) {
			auto idx = (static_cast<const char*>(pos) -
									static_cast<const char*>(ptr_)) /
								 buf_size_;
			return bufs_[idx];
		}
		Buffer* begin() { return bufs_; }
		Buffer* end() { return bufs_ + buf_num_; }

	 private:
	 	uint32_t buf_size_;
		uint32_t buf_num_;
		void* ptr_;
		Buffer* bufs_;
	};

	QueuePair* CreateQP(ibv_qp_type type, int ib_port,
											ibv_srq* srq, ibv_cq* scq,
											ibv_cq* rcq, uint32_t max_send,
											uint32_t max_recv, uint32_t qkey=0);
	int GetLid(int port);
	Buffer* TryReceive(QueuePair* qp, Address* peer_addr=nullptr);
	Buffer* Receive(QueuePair* qp, Address* peer_addr=nullptr);
	void PostReceive(QueuePair* qp, Buffer* b);
	void PostSRQReceive(ibv_srq* srq, Buffer* b);
	void PostSend(QueuePair* qp, Buffer* b, uint32_t len,
								const Address* peer_addr=nullptr,
								uint32_t peer_qkey=0);
  void PostSendAndWait(QueuePair* qp, Buffer* b, uint32_t len,
											 const Address* peer_addr=nullptr,
											 uint32_t peer_qkey=0);
  ibv_cq* CreateCQ(int min_entries) {
		return ibv_create_cq(dev_.ctx(), min_entries, nullptr, nullptr, 0);
	}
	ibv_srq* CreateSRQ(uint32_t max_wr, uint32_t max_sge) {
		ibv_srq_init_attr sia;
		memset(&sia, 0, sizeof(sia));
		sia.srq_context = dev_.ctx();
		sia.attr.max_wr = max_wr;
		sia.attr.max_sge = max_sge;
		return ibv_create_srq(pd_.pd(), &sia);
	}
	int PollCQ(ibv_cq* cq, int entry_num, ibv_wc* ret) {
		return ibv_poll_cq(cq, entry_num, ret);
	}
  
	Device& dev() { return dev_; }
	const Device& dev() const { return dev_; }
	ProtectionDomain& pd() { return pd_; }
	const ProtectionDomain& pd() const { return pd_; }

 private:
 	Device dev_;
	ProtectionDomain pd_;
};

} // namespace nvds

#endif // _NVDS_INFINIBAND_H_
