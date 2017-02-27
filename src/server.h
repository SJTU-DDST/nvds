#ifndef _NVDS_SERVER_H_
#define _NVDS_SERVER_H_

#include "common.h"
#include "object.h"
#include "tablet.h"

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
  uint32_t server_id;
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

class Server {
 public:
  using BackupList = std::vector<uint32_t>;
  Server(const BackupList& backup_list)
      : backup_list_(backup_list) {}
  ~Server() {}
  DISALLOW_COPY_AND_ASSIGN(Server);

  uint32_t id() const { return id_; }

  // Randomly choose a server for serving
  Server* GetRandomBackup();
  const Server* GetRandomBackup() const {
    return const_cast<Server*>(this)->GetRandomBackup();
  }

 private:
  uint32_t id_;

  BackupList backup_list_;
};

} // namespace nvds

#endif // _NVDS_SERVER_H_
