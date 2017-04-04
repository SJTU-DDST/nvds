#ifndef _NVDS_BASIC_SERVER_H_
#define _NVDS_BASIC_SERVER_H_

#include "session.h"

#include <boost/asio.hpp>

namespace nvds {

class BasicServer {
 public:
  BasicServer(uint16_t port);
  virtual ~BasicServer() {}

  virtual void Run() = 0;
  void Accept(MessageHandler msg_handler);

 protected:
  void RunService() {
    tcp_service_.run();
  }

  boost::asio::io_service tcp_service_;
  boost::asio::ip::tcp::acceptor tcp_acceptor_;
  boost::asio::ip::tcp::socket conn_sock_;
};

} // namespace nvds

#endif // _NVDS_BASIC_SERVER_H_
