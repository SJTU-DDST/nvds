#include "index.h"

namespace nvds {

void IndexManager::AddServer(ServerId server_id) {

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


} // namespace nvds
