/*
 * Mapping items to servers
 */

#ifndef _NVDS_INDEX_H_
#define _NVDS_INDEX_H_

#include "common.h"
#include "hash.h"
#include "infiniband.h"
#include "server.h"

#include <map>
#include <unordered_map>

namespace nvds {
/*
class IndexManager {
 public:
  static const uint32_t kVirtualNodeNum = 128;
  using KeyServerMap = std::map<KeyHash, ServerId>;
  struct ServerInfo {
    ServerId id;
    std::string addr; // ip address
    KeyServerMap::iterator vnodes[kVirtualNodeNum];
  };
  using ServerInfoMap = std::unordered_map<ServerId, ServerInfo>;
  
  IndexManager() {}
  ~IndexManager() {}
  DISALLOW_COPY_AND_ASSIGN(IndexManager);

  void AddServer(ServerId server_id, std::string addr);
  void RemoveServer(ServerId server_id);
  void ShrinkServer(ServerId server_id);
  std::vector<KeyHashRange> GetServerRanges(ServerId server_id);
  ServerId GetServer(KeyHash key_hash) const;

 private:
  KeyServerMap key_server_map_;
  ServerInfoMap server_info_map_;
};
*/

class IndexManager {
 public:
  IndexManager() {}
  ~IndexManager() {}
  DISALLOW_COPY_AND_ASSIGN(IndexManager);

  const ServerInfo& AddServer(const std::string& addr,
                              const Infiniband::Address& ib_addr);
  const ServerInfo& GetServer(KeyHash key_hash) const {
    auto id = GetServerId(key_hash);
    const auto& ans = servers_[id];
    assert(ans.id == id);
    return ans;
  }
  ServerId GetServerId(KeyHash key_hash) const {       
    return GetTablet(key_hash).server_id;
  }
  // Get the id of the tablet that this key hash locates in.
  TabletId GetTabletId(KeyHash key_hash) const {
    uint32_t idx = key_hash / ((1.0 + kMaxKeyHash) / kNumTablets);
    return key_tablet_map_[idx];
  }
  const TabletInfo& GetTablet(KeyHash key_hash) const {
    return tablets_[GetTabletId(key_hash)];
  }
  TabletInfo& GetTablet(KeyHash key_hash) {
    return tablets_[GetTabletId(key_hash)];
  }
  void AssignBackups();

 private:
  static ServerId AllocServerId() {
    static ServerId id = 0;
    return id++;
  }
  static TabletId AllocTabletId() {
    static TabletId id = 0;
    return id++;
  }

  std::array<TabletId, kNumTablets> key_tablet_map_;
  std::array<TabletInfo, kNumTablets * (kNumReplicas + 1)> tablets_;
  std::array<ServerInfo, kNumServers> servers_;
};

} // namespace nvds

#endif // _NVDS_INDEX_H_
