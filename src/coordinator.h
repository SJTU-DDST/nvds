#ifndef _NVDS_COORDINATOR_H_
#define _NVDS_COORDINATOR_H_

#include "index.h"
#include "message.h"

#include <boost/asio.hpp>

namespace nvds {

/*
 * Coordinator do not serve any frequent reuqests from client,
 * like the get/put operation. It accepts various messages,
 * controls server recovery/joining/removing and failure
 * detection. It maintains the metadata of the whole cluster.
 *
 * At any given time, there is at most one active coordinator.
 * But several standby coordinators are preparing to take in charge
 * in case of the primary coordinator is down.
 */
class Coordinator {
 public:
  using RawData = uint8_t;

  Coordinator();
  ~Coordinator() {}

  uint32_t server_num() const { return server_num_; }
  uint64_t total_storage() const { return total_storage_; }

  IndexManager& index_manager() { return index_manager_; }
  const IndexManager& index_manager() const { return index_manager_; }

  void Run();

  class Session : public std::enable_shared_from_this<Session> {
   public:
    Session(boost::asio::ip::tcp::socket conn_sock)
        : conn_sock_(std::move(conn_sock)) {}
    ~Session() {}

    void Start();
    void ReadRequest();
    void WriteResponse();

   private:
    int32_t Read(RawData* raw_data, uint32_t len);
    int32_t Write(RawData* raw_data, uint32_t len);

   private:
    boost::asio::ip::tcp::socket conn_sock_;
    static const uint32_t kRawDataSize = 1024 * 16;
    RawData raw_data_[kRawDataSize];
    Message msg;
  };

 private:
  void Accept();

 private:
  uint32_t server_num_;
  uint64_t total_storage_;

  IndexManager index_manager_;

  boost::asio::io_service tcp_service_;
  boost::asio::ip::tcp::acceptor tcp_acceptor_;
  boost::asio::ip::tcp::socket conn_sock_;
};

} // namespace nvds

#endif // NVDS_COORDINATOR_H_
