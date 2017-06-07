#ifndef _NVDS_CLIENT_H_
#define _NVDS_CLIENT_H_

#include "common.h"
#include "index.h"
#include "infiniband.h"
#include "session.h"

namespace nvds {

class Client {
 public:
  Client(const std::string& coord_addr);
  ~Client();
  DISALLOW_COPY_AND_ASSIGN(Client);

  // Get value by the key, return empty string if error occurs.
  std::string Get(const std::string& key);
  std::string Get(const char* key, size_t key_len);

  // Store key/value pair to the cluster, return if operation succeed.
  bool Put(const std::string& key, const std::string& val);
  bool Put(const char* key, size_t ley_len, const char* val, size_t val_len);
  
  // Delete item indexed by the key, return if operation succeed.
  bool Del(const std::string& key);
  bool Del(const char* key, size_t key_len);

  // Statistic
  size_t num_send() const { return num_send_; }

 private:
  static const uint32_t kMaxIBQueueDepth = 128;
  static const uint32_t kSendBufSize = 1024 * 2 + 128;
  static const uint32_t kRecvBufSize = 1024 + 128;
  // May throw exception `boost::system::system_error`
  tcp::socket Connect(const std::string& coord_addr);
  void Close() {}
  void Join();
  
  boost::asio::io_service tcp_service_;
  Session session_;
  IndexManager index_manager_;

  // Infiniband
  Infiniband ib_;
  Infiniband::RegisteredBuffers send_bufs_;
  Infiniband::RegisteredBuffers recv_bufs_;
  Infiniband::QueuePair* qp_;

  // Statistic 
  size_t num_send_ {0};
};

} // namespace nvds

#endif // _NVDS_CLIENT_H_
