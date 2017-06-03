#include "basic_server.h"

#include "session.h"

using boost::asio::ip::tcp;

namespace nvds {

BasicServer::BasicServer(uint16_t port)
    : tcp_acceptor_(tcp_service_,
                    tcp::endpoint(tcp::v4(), port)),
      conn_sock_(tcp_service_) {
}

void BasicServer::Accept(MessageHandler recv_msg_handler,
                         MessageHandler send_msg_handler) {
  tcp_acceptor_.async_accept(conn_sock_,
      [this, recv_msg_handler, send_msg_handler](boost::system::error_code err) {
        if (!err) {
          std::make_shared<Session>(std::move(conn_sock_),
              recv_msg_handler, send_msg_handler)->Start();
        } else {
          NVDS_ERR(err.message().c_str());
        }
        Accept(recv_msg_handler, send_msg_handler);
      });
}

} // namespace nvds
