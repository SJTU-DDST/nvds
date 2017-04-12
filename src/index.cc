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
  auto& server = servers_[id];
  server.id = id;
  server.active = true;
  server.addr = addr;
  server.ib_addr = ib_addr;

  ++num_servers_;
  return server;
}

} // namespace nvds
