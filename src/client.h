#ifndef _NVDS_CLIENT_H_
#define _NVDS_CLIENT_H_

#include "common.h"
#include "index.h"
#include "session.h"

namespace nvds {

class Client {
 public:
  Client(const std::string& coord_addr);
  ~Client() { Close(); }
  DISALLOW_COPY_AND_ASSIGN(Client);

  enum class Status: uint8_t {
    OK,
    ERROR,
    REJECT,
  };

  // Get value by the key
  std::string Get(const std::string& key);

  // Store key/value pair to the cluster
  Status Put(const std::string& key, const std::string& value);
  
  // Delete item indexed by the key
  Status Delete(const std::string& key);

 private:
  // May Throw exception ``
  tcp::socket Connect(const std::string& coordinator_addr);
  void Close() {}
  void Join();
  boost::asio::io_service tcp_service_;
  Session session_;
  IndexManager index_manager_;
};

} // namespace nvds

#endif // _NVDS_CLIENT_H_
