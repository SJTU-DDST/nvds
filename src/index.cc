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
                                          nlohmann::json& msg_body) {
  auto id = AllocServerId();
  servers_[id] = {id, true, addr, msg_body["ib_addr"]};
  Infiniband::Address ib_addr = msg_body["ib_addr"];
  std::array<uint64_t, kNumTabletAndBackupsPerServer> vaddrs = msg_body["tablets_vaddr"];
  std::array<uint32_t, kNumTabletAndBackupsPerServer> rkeys  = msg_body["tablets_rkey"];
  servers_[id] = {id, true, addr, ib_addr};

  /*
  for (size_t k = 0; k < kNumReplicas + 1; ++k) {
    auto i = id;
    for (TabletId j = 0; j < kNumTabletsPerServer; ++j) {
      auto tablet_id = CalcTabletId(k, i, j);
      tablets_[tablet_id] = {tablet_id, i, k > 0};
      tablets_[tablet_id].vaddr = vaddrs[k * kNumTabletsPerServer + j];
      tablets_[tablet_id].rkey  = rkeys[k * kNumTabletsPerServer + j];
      if (k > 0) {
        TabletId master_id = CalcTabletId(0, i, j);
        // Backup tablets locate on different servers!
        TabletId backup_id = CalcTabletId(k, (i + k) % kNumServers, j);
        tablets_[master_id].backups[k - 1] = backup_id;
        tablets_[backup_id].master = master_id;
      } else {
        key_tablet_map_[tablet_id] = tablet_id;
      }
      servers_[i].tablets[k * kNumTabletsPerServer + j] = tablet_id;
    }
  }
  */

  auto i = id;
  for (uint32_t j = 0; j < kNumReplicas; ++j) {
    for (uint32_t k = 0; k < kNumTabletsPerServer; ++k) {
      auto tablet_id = CalcTabletId(i, j, k);
      auto tablet_idx = CalcTabletId(0, j, k);
      tablets_[tablet_id] = {tablet_id, i, j > 0};
      tablets_[tablet_id].vaddr = vaddrs[tablet_idx];
      tablets_[tablet_id].rkey  = rkeys[tablet_idx];

      if (j == 0) {
        key_tablet_map_[tablet_id] = tablet_id;
      } else {
        auto backup_id = tablet_id;
        auto master_id = CalcTabletId((kNumServers + i - j) % kNumServers, 0, k);
        // It is tricky here
        tablets_[master_id].backups[j-1] = backup_id;
        tablets_[backup_id].master = master_id;
      }
      servers_[i].tablets[tablet_idx] = tablet_id;
    }
  }

  return servers_[id];
}

/*
void IndexManager::AssignTablets() {
  for (size_t k = 0; k < kNumReplicas + 1; ++k) {
    for (ServerId i = 0; i < kNumServers; ++i) {
      for (TabletId j = 0; j < kNumTabletsPerServer; ++j) {
        auto tablet_id = CalcTabletId(k, i, j);
        tablets_[tablet_id] = {tablet_id, i, k > 0};
        if (k > 0) {
          TabletId master_id = CalcTabletId(0, i, j);
          // Backup tablets locate on different servers!
          TabletId backup_id = CalcTabletId(k, (i + k) % kNumServers, j);
          tablets_[master_id].backups[k - 1] = backup_id;
          tablets_[backup_id].master = master_id;
        } else {
          key_tablet_map_[tablet_id] = tablet_id;
        }
        servers_[i].tablets[k * kNumTabletsPerServer + j] = tablet_id;
      }
    }
  }
}
*/

} // namespace nvds
