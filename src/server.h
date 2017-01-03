#ifndef _NVDS_SERVER_H_
#define _NVDS_SERVER_H_

#include "common.h"

namespace nvds {

class Server {
public:
  using BackupList = std::vector<uint32_t>;

public:
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
