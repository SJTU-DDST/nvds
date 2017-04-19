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

  // Get value by the key
  std::string Get(const std::string& key);

  // Store key/value pair to the cluster
  bool Put(const std::string& key, const std::string& value);
  
  // Delete item indexed by the key
  bool Delete(const std::string& key);

 private:
  static const uint32_t kSendBufSize = 1024 * 2 + 128;
  static const uint32_t kRecvBufSize = 1024 + 128;
  // May throw exception `boost::system::system_error`
  tcp::socket Connect(const std::string& coord_addr);
  void Close() {}
  void Join();
  // Init infiniband devices
  void InitIB();
  Infiniband::QueuePair* GetQP(const ServerInfo& server) {
    return qps_[server.id];
  }
  Infiniband::Buffer* GetBuffer(Infiniband::RegisteredBuffers& bufs,
                                const ServerInfo& server) {
    return &bufs[server.id];
  }
  Session session_;
  IndexManager index_manager_;

  // Infiniband
  Infiniband ib_;
  Infiniband::RegisteredBuffers send_bufs_;
  Infiniband::RegisteredBuffers recv_bufs_;
  std::array<Infiniband::QueuePair*, kNumServers> qps_;
  ibv_cq* rcq_;
  ibv_cq* scq_;
};

} // namespace nvds

#endif // _NVDS_CLIENT_H_
