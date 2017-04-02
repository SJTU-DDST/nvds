#include "basic_server.h"

#include "config.h"
#include "session.h"

using boost::asio::ip::tcp;

namespace nvds {

BasicServer::BasicServer()
    : tcp_acceptor_(tcp_service_,
                    tcp::endpoint(tcp::v4(),
                    Config::coord_port())),
      conn_sock_(tcp_service_) {
}

/*
virtual void BasicServer::Run() {
  Accept();
  tcp_service_.run();
}
*/

void BasicServer::Accept(MessageHandler msg_handler) {
  tcp_acceptor_.async_accept(conn_sock_,
    [this, &msg_handler](boost::system::error_code err) {
      if (!err) {
        std::make_shared<Session>(std::move(conn_sock_), msg_handler)->Start();
      } else {
        NVDS_ERR("accept connection failed");
      }
      Accept(msg_handler);
    });
}

} // namespace nvds
