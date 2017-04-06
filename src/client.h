#ifndef _NVDS_CLIENT_H_
#define _NVDS_CLIENT_H_

#include "index.h"

#include <cstdint>
#include <string>

namespace nvds {

class Client {
 public:
  Client(): index_manager_(nullptr) {}
  ~Client() { delete index_manager_; }
  Client(const Client& other) = delete;

  enum class Status: uint8_t {
    OK,
    ERROR,
    REJECT,
  };

  // Connection to the cluster, return connection status.
  Status Connect(const std::string& coordinator_addr);
  Status Close();

  // Get value by the key
  std::string Get(const std::string& key);

  // Store key/value pair to the cluster
  Status Put(const std::string& key, const std::string& value);
  
  // Delete item indexed by the key
  Status Delete(const std::string& key);

 private:
  IndexManager* index_manager_;
};

} // namespace nvds

#endif // _NVDS_CLIENT_H_
