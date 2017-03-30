#include "coordinator.h"

#include "config.h"
#include "message.h"

using boost::asio::ip::tcp;

namespace nvds {

Coordinator::Coordinator()
    : server_num_(0), total_storage_(0),
      tcp_acceptor_(tcp_service_,
                    tcp::endpoint(tcp::v4(),
                    Config::coord_port())),
      conn_sock_(tcp_service_) {
  
}

void Coordinator::Accept() {
  tcp_acceptor_.async_accept(conn_sock_,
      [this](boost::system::error_code err) {
        if (!err) {
          std::make_shared<Session>(std::move(conn_sock_))->Start();
        }
        Accept();
      });
}

void Coordinator::Run() {
  Accept();
  tcp_service_.run();
}

void Coordinator::Session::Start() {
  ReadRequest();
}

void Coordinator::Session::ReadRequest() {
  auto self = shared_from_this(); // Avoid out-of-range of this session
  auto body_handler = [this, self](
      const boost::system::error_code& error,
      size_t bytes_transferred) {
    if (!error) {
      
    } else {
      NVDS_ERR(error.message().c_str());
    }
  };
  auto header_handler = [this, self, body_handler](
      const boost::system::error_code& error,
      size_t bytes_transferred) {
    if (!error) {
      msg = reinterpret_cast<Message::Format*>(raw_data_);
      
      size_t len = msg.body_len();
      assert(len + Message::kHeaderLen <= kRawDataSize);
      boost::asio::async_read(conn_sock_,
          boost::asio::buffer(raw_data_, len), body_handler);    
    } else {
      NVDS_ERR(error.message().c_str());
    }
  };
  
  size_t len = Message::kHeaderLen;
  assert(len <= kRawDataSize);
  boost::asio::async_read(conn_sock_,
      boost::asio::buffer(raw_data_, len), header_handler);
}

void Coordinator::Session::WriteResponse() {

}

int32_t Coordinator::Session::Read(RawData* raw_data, uint32_t len) {
  return 0;
}

int32_t Coordinator::Session::Write(RawData* raw_data, uint32_t len) {
  return 0;
}

} // namespace nvds
