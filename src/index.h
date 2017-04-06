/*
 * The implementation of consistent hashing.
 */

#ifndef _NVDS_INDEX_H_
#define _NVDS_INDEX_H_

#include "common.h"
#include "hash.h"

#include <map>
#include <unordered_map>

namespace nvds {

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

  void AddServer(ServerId server_id);
  void RemoveServer(ServerId server_id);
  void ShrinkServer(ServerId server_id);
  std::vector<KeyHashRange> GetServerRanges(ServerId server_id);
  ServerId GetServer(KeyHash key_hash) const;

 private:
  KeyServerMap key_server_map_;
  ServerInfoMap server_info_map_;
};

} // namespace nvds

#endif // _NVDS_INDEX_H_
