#ifndef _NVDS_INDEX_H_
#define _NVDS_INDEX_H_

#include "common.h"

#include <map>
#include <unordered_map>

namespace nvds {

class Server;
// Mapping from object key hash to server
using IndexMap = std::map<KeyHash, Server*>;
// Mapping from server id to server
using ServerMap = std::unordered_map<uint32_t, Server*>;

/*
 * Management mapping from key to server, add/remove server,
 * Narrow a server's key range if it reaches its capacity.
 * These updates will make cache of mapping in client side invalid.
 */
class IndexManager {
 public:
  IndexManager() {}
  ~IndexManager() {}
  IndexManager(const IndexManager& other) = delete;

  void AddServer(uint32_t server_id);
  void RemoveServer(uint32_t server_id);
  void NarrowServer(uint32_t server_id);

  const Server* GetServer(KeyHash key_hash) const;
  Server* GetServer(KeyHash key_hash) {
    auto ret = const_cast<const IndexManager*>(this)->GetServer(key_hash);
    return const_cast<Server*>(ret);
  }


 private:
  IndexMap index_map_;
  ServerMap server_map_;
};

} // namespace nvds

#endif // _NVDS_INDEX_H_
