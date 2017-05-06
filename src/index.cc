#include "index.h"

namespace nvds {

const ServerInfo& IndexManager::AddServer(const std::string& addr,
                                          nlohmann::json& msg_body) {
  auto id = AllocServerId();
  Infiniband::Address ib_addr = msg_body["ib_addr"];
  servers_[id] = {id, true, addr, ib_addr};
  std::vector<Infiniband::QueuePairInfo> qpis = msg_body["tablet_qpis"];
  
  auto i = id;
  for (uint32_t j = 0; j < kNumReplicas + 1; ++j) {
    for (uint32_t k = 0; k < kNumTabletsPerServer; ++k) {
      auto tablet_id = CalcTabletId(i, j, k);
      auto tablet_idx = CalcTabletId(0, j, k);
      tablets_[tablet_id].id = tablet_id;
      tablets_[tablet_id].server_id = i;
      tablets_[tablet_id].is_backup = j > 0;

      copy(qpis.begin() + tablet_idx * kNumReplicas,
           qpis.begin() + (tablet_idx + 1) * kNumReplicas,
           tablets_[tablet_id].qpis.begin());

      if (!tablets_[tablet_id].is_backup) {
        key_tablet_map_[tablet_id] = tablet_id;
      } else {
        auto backup_id = tablet_id;
        auto master_id = CalcTabletId((kNumServers + i - j) % kNumServers, 0, k);
        // It is tricky here
        assert(backup_id != 0);
        tablets_[master_id].backups[j-1] = backup_id;
        tablets_[backup_id].master = master_id;
      }
      servers_[i].tablets[tablet_idx] = tablet_id;
    }
  }

  return servers_[id];
}

} // namespace nvds
