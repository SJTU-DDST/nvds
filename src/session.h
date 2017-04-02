#ifndef _NVDS_SESSION_H_
#define _NVDS_SESSION_H_

#include "message.h"

#include <boost/asio.hpp>
#include <boost/function.hpp>

namespace nvds {

class Session;
using MessageHandler = boost::function<void(Session&)>;

class Session : public std::enable_shared_from_this<Session> {
 public:
  using RawData = uint8_t;

  Session(boost::asio::ip::tcp::socket conn_sock,
          MessageHandler msg_handler)
      : conn_sock_(std::move(conn_sock)),
        msg_handler_(std::move(msg_handler)) {}
  ~Session() {}

  void Start();
  void RecvMessage();
  void SendMessage();

  Message& msg() { return msg_; }
  const Message& msg() const { return msg_; }

  //private:
  // int32_t Read(RawData* raw_data, uint32_t len);
  // int32_t Write(RawData* raw_data, uint32_t len);

  private:
  boost::asio::ip::tcp::socket conn_sock_;
  MessageHandler msg_handler_;
  static const uint32_t kRawDataSize = 1024 * 16;
  RawData raw_data_[kRawDataSize];
  Message msg_;
};

} // namespace nvds

#endif // _NVDS_SESSION_H_
