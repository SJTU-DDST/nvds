/*
 * Mapping items to servers
 */

#ifndef _NVDS_INDEX_H_
#define _NVDS_INDEX_H_

#include "common.h"
#include "hash.h"
#include "infiniband.h"
#include "json.hpp"
#include "message.h"

#include <map>
#include <unordered_map>

namespace nvds {

class IndexManager {
  friend void to_json(nlohmann::json& j, const IndexManager& im);
  friend void from_json(const nlohmann::json& j, IndexManager& im);

 public:
  IndexManager() {}
  ~IndexManager() {}

  const ServerInfo& AddServer(const std::string& addr,
                              nlohmann::json& msg_body);
  const ServerInfo& GetServer(KeyHash key_hash) const {
    auto id = GetServerId(key_hash);
    const auto& ans = GetServer(id);
    assert(ans.id == id);
    return ans;
  }
  const ServerInfo& GetServer(ServerId id) const {
    return servers_[id];
  }
  ServerId GetServerId(KeyHash key_hash) const {       
    return GetTablet(key_hash).server_id;
  }
  // Get the id of the tablet that this key hash locates in.
  TabletId GetTabletId(KeyHash key_hash) const {
    uint32_t idx = key_hash / ((1.0 + kMaxKeyHash) / key_tablet_map_.size());
    return key_tablet_map_[idx];
  }
  const TabletInfo& GetTablet(KeyHash key_hash) const {
    return tablets_[GetTabletId(key_hash)];
  }
  //TabletInfo& GetTablet(KeyHash key_hash) {
  //  return tablets_[GetTabletId(key_hash)];
  //}
  const TabletInfo& GetTablet(TabletId id) const {
    return tablets_[id];
  }

  // DEBUG
  void PrintTablets() const;
  
 private:
  static ServerId AllocServerId() {
    static ServerId id = 0;
    return id++;
  }

  // `r_idx`: replication index; `s_idx`: server index; `t_idx`: tablet index;
  static uint32_t CalcTabletId(uint32_t s_idx, uint32_t r_idx, uint32_t t_idx) {
    return s_idx * kNumTabletAndBackupsPerServer + r_idx * kNumTabletsPerServer + t_idx;
  }

  std::array<TabletId, kNumTablets> key_tablet_map_;
  std::array<TabletInfo, kNumTabletAndBackups> tablets_;
  std::array<ServerInfo, kNumServers> servers_;
};

} // namespace nvds

#endif // _NVDS_INDEX_H_
