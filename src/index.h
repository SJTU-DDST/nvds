/*
 * Mapping item to server
 */

#ifndef _NVDS_INDEX_H_
#define _NVDS_INDEX_H_

#include "common.h"
#include "hash.h"

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
  struct ServerInfo {
    ServerId id;
    std::string addr; // ip address
    ServerId replicas[kNumReplica];
  };
  IndexManager(uint32_t total_server)
      : num_servers_(0), servers_(total_server) {}

  const ServerInfo& AddServer(std::string addr) {
    auto id = AllocServerId();
    auto& server = servers_[id];
    server.id = id;
    server.addr = std::move(addr);
    for (size_t i = 0; i < kNumReplica; ++i) {
      server.replicas[i] = (id + 1 + i) % servers_.size();
    }
    ++num_servers_;
    return server;
  }

  const ServerInfo& GetServer(KeyHash key_hash) const {
    assert(num_servers_ != servers_.size());
    return servers_[key_hash % servers_.size()];
  }
  ServerId GetServerId(KeyHash key_hash) const {
    return GetServer(key_hash).id;
  }
 
 private:
  static ServerId AllocServerId() {
    static ServerId id = 0;
    return id++;
  }

  uint32_t num_servers_;
  std::vector<ServerInfo> servers_;
};

} // namespace nvds

#endif // _NVDS_INDEX_H_
