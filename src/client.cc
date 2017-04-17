#include "client.h"

namespace nvds {

using json = nlohmann::json;

Client::Client(const std::string& coord_addr)
    : session_(Connect(coord_addr)) {
  Join();
}

void Client::Join() {
  Message msg {
    Message::Header {Message::SenderType::CLIENT, Message::Type::REQ_JOIN, 0},
    json().dump()
  };
  session_.SendMessage(msg);
  msg = session_.RecvMessage();
  assert(msg.sender_type() == Message::SenderType::COORDINATOR);
  assert(msg.type() == Message::Type::RES_JOIN);
  auto j_body = json::parse(msg.body());
  index_manager_ = j_body["index_manager"];
}

tcp::socket Client::Connect(const std::string& coord_addr) {
  boost::asio::io_service tcp_service;
  tcp::resolver resolver {tcp_service};
  tcp::socket conn_sock {tcp_service};
  tcp::resolver::query query {coord_addr, std::to_string(kCoordPort)};
  auto ep = resolver.resolve(query);
  boost::asio::connect(conn_sock, ep);
  return conn_sock;
}

std::string Client::Get(const std::string& key) {
  return "";
}

Client::Status Client::Put(const std::string& key, const std::string& value) {
  return Status::OK;
}

Client::Status Client::Delete(const std::string& key) {
  return Status::OK;
}

} // namespace nvds
