#include "index.h"

namespace nvds {

/*
void IndexManager::AddServer(ServerId server_id, std::string addr) {
  auto& server_info = server_info_map_[server_id];

  // Generate vritual nodes

  server_info.id = server_id;
  server_info.addr = std::move(addr);
}

// Get the server that the `key_hash` will fall in.
ServerId IndexManager::GetServer(KeyHash key_hash) const {
  assert(key_server_map_.size() > 0);
  auto iter = key_server_map_.upper_bound(key_hash);
  if (iter == key_server_map_.begin()) {
    iter = key_server_map_.end();
  }
  return (--iter)->second;
}

std::vector<KeyHashRange> IndexManager::GetServerRanges(ServerId server_id) {
  std::vector<KeyHashRange> ret;
  return ret;
}
*/

const ServerInfo& IndexManager::AddServer(const std::string& addr,
                                          const Infiniband::Address& ib_addr) {
  auto id = AllocServerId();
  servers_[id] = {id, true, addr, ib_addr};

  return servers_[id];
}

void IndexManager::AssignTablets() {
  for (size_t k = 0; k < kNumReplicas + 1; ++k) {
    for (ServerId i = 0; i < kNumServers; ++i) {
      for (TabletId j = 0; j < kNumTabletsPerServer; ++j) {
        auto tablet_id = AllocTabletId();
        tablets_[tablet_id] = {tablet_id, i, k > 0};
        if (k > 0) {
          TabletId master_id = i * kNumTabletsPerServer + j;
          // Backup tablets locate on different servers!
          TabletId backup_id = k * kNumTablets +
              ((i + k) % kNumServers) * kNumTabletsPerServer + j;
          tablets_[master_id].backups[k - 1] = backup_id;
          tablets_[backup_id].master = master_id;
        } else {
          key_tablet_map_[tablet_id] = tablet_id;
          servers_[i].tablets[j] = tablet_id;
        }
      }
    }
  }
}

} // namespace nvds
