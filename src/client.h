#ifndef _NVDS_CLIENT_H_
#define _NVDS_CLIENT_H_

#include "common.h"
#include "index.h"
#include "infiniband.h"
#include "request.h"
#include "response.h"
#include "session.h"

namespace nvds {

class Client {
  using Buffer = Infiniband::Buffer;
 public:
  Client(const std::string& coord_addr);
  ~Client();
  DISALLOW_COPY_AND_ASSIGN(Client);

  // Get value by the key, return empty string if error occurs.
  // Throw: TransportException
  std::string Get(const std::string& key) {
    return Get(key.c_str(), key.size());
  }
  std::string Get(const char* key, size_t key_len) {
    auto rb = RequestAndWait(key, key_len, nullptr, 0, Request::Type::GET);
    auto resp = rb->MakeResponse();
    std::string ans {resp->val, resp->val_len};
    recv_bufs_.Free(rb);
    return ans;
  }

  // Insert key/value pair to the cluster, return if operation succeed.
  // Throw: TransportException
  bool Put(const std::string& key, const std::string& val) {
    return Put(key.c_str(), key.size(), val.c_str(), val.size());
  }
  bool Put(const char* key, size_t key_len, const char* val, size_t val_len) {
    auto rb = RequestAndWait(key, key_len, val, val_len, Request::Type::PUT);
    bool ans = rb->MakeResponse()->status == Status::OK;
    recv_bufs_.Free(rb);
    return ans;
  }

  // Add key/value pair to the cluster,
  // return false if there is already the same key;
  // Throw: TransportException
  bool Add(const std::string& key, const std::string& val) {
    return Add(key.c_str(), key.size(), val.c_str(), val.size());
  }
  bool Add(const char* key, size_t key_len, const char* val, size_t val_len) {
    auto rb = RequestAndWait(key, key_len, val, val_len, Request::Type::ADD);
    // FIXME(wgtdkp): what about Status::NO_MEM?
    bool ans = rb->MakeResponse()->status == Status::OK;
    recv_bufs_.Free(rb);
    return ans;
  }

  // Delete item indexed by the key, return if operation succeed.
  // Throw: TransportException
  bool Del(const std::string& key) {
    return Del(key.c_str(), key.size());
  }
  bool Del(const char* key, size_t key_len) {
    auto rb = RequestAndWait(key, key_len, nullptr, 0, Request::Type::DEL);
    bool ans = rb->MakeResponse()->status == Status::OK;
    recv_bufs_.Free(rb);
    return ans;
  }

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
  Buffer* RequestAndWait(const char* key, size_t key_len,
      const char* val, size_t val_len, Request::Type type);

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
