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

void Coordinator::Run() {
  Accept();
  tcp_service_.run();
}

void Coordinator::HandleMessage(Session& session) {
  switch (session.msg().sender_type()) {
  case Message::SenderType::SERVER:
    HandleMessageFromServer(session);
    break;
  case Message::SenderType::CLIENT:
    HandleMessageFromClient(session);
    break;
  case Message::SenderType::COORDINATOR:
    assert(false);
  }
}

void Coordinator::HandleMessageFromServer(Session& session) {
  const auto& msg = session.msg();
  switch (msg.type()) {
  case Message::Type::REQ_JOIN:
    break;
  case Message::Type::REQ_LEAVE:
    break;
  case Message::Type::ACK_REJECT:
    break;
  case Message::Type::ACK_ERROR:
    break;
  case Message::Type::ACK_OK:
    break;
  default:
    assert(false);
  }
}

void Coordinator::HandleMessageFromClient(Session& session) {
  const auto& msg = session.msg();
  switch (msg.type()) {
  case Message::Type::REQ_JOIN:
    break;
  case Message::Type::REQ_LEAVE:
    break;
  case Message::Type::ACK_REJECT:
    break;
  case Message::Type::ACK_ERROR:
    break;
  case Message::Type::ACK_OK:
    break;
  default:
    assert(false);
  }
}

void Coordinator::Accept() {
  tcp_acceptor_.async_accept(conn_sock_,
      [this](boost::system::error_code err) {
        if (!err) {
          std::make_shared<Session>(
              std::move(conn_sock_),
              std::bind(&Coordinator::HandleMessage,
                        this, std::placeholders::_1))->Start();
        }
        Accept();
      });
}

void Coordinator::Session::Start() {
  RecvMessage();
}

void Coordinator::Session::RecvMessage() {
  auto self = shared_from_this(); // Avoid out-of-range of this session
  auto body_handler = [this, self](
      const boost::system::error_code& err,
      size_t bytes_transferred) {
    if (!err) {
      // TODO(wgtdkp): handle this message
      msg_handler_(*this);
    } else {
      NVDS_ERR(err.message().c_str());
    }
  };
  auto header_handler = [this, self, body_handler](
      const boost::system::error_code& err,
      size_t bytes_transferred) {
    if (!err) {
      msg_ = reinterpret_cast<Message::Format*>(raw_data_);
      // Read message body
      size_t len = msg_.body_len();
      assert(len + Message::kHeaderLen <= kRawDataSize);
      boost::asio::async_read(conn_sock_,
          boost::asio::buffer(raw_data_, len), body_handler);    
    } else {
      NVDS_ERR(err.message().c_str());
    }
  };
  
  // Read message header
  size_t len = Message::kHeaderLen;
  assert(len <= kRawDataSize);
  boost::asio::async_read(conn_sock_,
      boost::asio::buffer(raw_data_, len), header_handler);
}

void Coordinator::Session::SendMessage() {

}

int32_t Coordinator::Session::Read(RawData* raw_data, uint32_t len) {
  return 0;
}

int32_t Coordinator::Session::Write(RawData* raw_data, uint32_t len) {
  return 0;
}

} // namespace nvds
