#ifndef _NVDS_SERVER_H_
#define _NVDS_SERVER_H_

#include "basic_server.h"
#include "common.h"
#include "object.h"
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
  //using BackupList = std::vector<ServerId>;

  Server(NVMPtr<NVMDevice> nvm, uint64_t nvm_size);
  ~Server() {}
  DISALLOW_COPY_AND_ASSIGN(Server);

  ServerId id() const { return id_; }

  // Randomly choose a server for serving
  Server* GetRandomBackup();
  const Server* GetRandomBackup() const {
    return const_cast<Server*>(this)->GetRandomBackup();
  }

  void Run() override;
  bool Join();
  void Leave();
  void Listening();

 private:
  void HandleRecvMessage(Session& session, std::shared_ptr<Message> msg);
  void HandleSendMessage(Session& session, std::shared_ptr<Message> msg);

  ServerId id_;
  uint64_t nvm_size_;  
  NVMPtr<NVMDevice> nvm_;
};

} // namespace nvds

#endif // _NVDS_SERVER_H_
