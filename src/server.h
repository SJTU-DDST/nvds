#ifndef _NVDS_SERVER_H_
#define _NVDS_SERVER_H_

#include "basic_server.h"
#include "common.h"
#include "index.h"
#include "infiniband.h"
#include "message.h"
#include "tablet.h"

#include <boost/asio.hpp>
#include <boost/function.hpp>

namespace nvds {

/*
 * The abstraction of NVM device. No instantiation is allowed.
 * For each server, there is only one NVMDevice object which is
 * mapped to the address passed when the server is started.
 */
PACKED(
struct NVMDevice {
  // The server id of the machine.
  // 0 means this NVMDevice is not used yet, all information are invalid.
  ServerId server_id;
  // The size in byte of the NVM device
  uint64_t size;
  // The key hash range
  KeyHash key_begin;
  KeyHash key_end;
  // The number of tablets
  uint32_t tablet_num;
  // The begin of tablets
  Tablet tablets[0];
  NVMDevice() = delete;
});

class Server : public BasicServer {
 public:
  Server(NVMPtr<NVMDevice> nvm, uint64_t nvm_size);
  ~Server() {
    delete qp_;
    ib_.DestroyCQ(scq_);
    ib_.DestroyCQ(rcq_);
  }
  DISALLOW_COPY_AND_ASSIGN(Server);

  ServerId id() const { return id_; }
  bool active() const { return active_; }  
  uint64_t nvm_size() const { return nvm_size_; }
  NVMPtr<NVMDevice> nvm() const { return nvm_; }

  void Run() override;
  bool Join();
  void Leave();
  void Listening();

 private:
  static const uint32_t kSendBufSize = 1024 + 128;
  static const uint32_t kRecvBufSize = 1024 * 2 + 128;
  void HandleRecvMessage(std::shared_ptr<Session> session,
                         std::shared_ptr<Message> msg);
  void HandleSendMessage(std::shared_ptr<Session> session,
                         std::shared_ptr<Message> msg);
  ServerId id_;
  bool active_;
  uint64_t nvm_size_;  
  NVMPtr<NVMDevice> nvm_;

  IndexManager index_manager_;

  // Infiniband
  Infiniband ib_;
  Infiniband::Address ib_addr_;
  Infiniband::RegisteredBuffers send_bufs_;
  Infiniband::RegisteredBuffers recv_bufs_;
  Infiniband::QueuePair* qp_;
  ibv_cq* rcq_;
  ibv_cq* scq_;
};

} // namespace nvds

#endif // _NVDS_SERVER_H_
