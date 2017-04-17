#ifndef _NVDS_SESSION_H_
#define _NVDS_SESSION_H_

#include "message.h"

#include <boost/asio.hpp>
#include <boost/function.hpp>

namespace nvds {

class Session;
using MessageHandler = boost::function<void(std::shared_ptr<Session>,
                                            std::shared_ptr<Message>)>;
using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
 public:
  explicit Session(tcp::socket conn_sock,
                   MessageHandler recv_msg_handler=0,
                   MessageHandler send_msg_handler=0)
      : conn_sock_(std::move(conn_sock)),
        recv_msg_handler_(recv_msg_handler),
        send_msg_handler_(send_msg_handler) {}
  ~Session() { /* Close socket automatically */ }

  // Start async message send/recv
  void Start();
  void AsyncRecvMessage(std::shared_ptr<Message> msg);
  void AsyncSendMessage(std::shared_ptr<Message> msg);
  // Throw exception `boost::system::system_error` on failure
  Message RecvMessage();
  // Throw exception `boost::system::system_error` on failure
  void SendMessage(const Message& msg);

  std::string GetPeerAddr() const {
    return conn_sock_.remote_endpoint().address().to_string();
  }

 private:
  tcp::socket conn_sock_;
  MessageHandler recv_msg_handler_;
  MessageHandler send_msg_handler_;
};

} // namespace nvds

#endif // _NVDS_SESSION_H_
