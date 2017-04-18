#include "client.h"

#include "request.h"

namespace nvds {

using json = nlohmann::json;

Client::Client(const std::string& coord_addr)
    : session_(Connect(coord_addr)),
      send_bufs_(ib_.pd(), Infiniband::kSendBufSize, kNumServers),
      recv_bufs_(ib_.pd(), Infiniband::kRecvBufSize, kNumServers, true) {
  InitIB();
  Join();
}

Client::~Client() {
  Close();
  ib_.DestroyCQ(scq_);
  ib_.DestroyCQ(rcq_);
  for (size_t i = 0; i < kNumServers; ++i) {
    delete qps_[i];
  }
}

void Client::InitIB() {
  scq_ = ib_.CreateCQ(1);
  rcq_ = ib_.CreateCQ(1);
  for (size_t i = 0; i < kNumServers; ++i) {
    qps_[i] = new Infiniband::QueuePair(ib_, IBV_QPT_RC, i,
                                        nullptr, scq_, rcq_, 128, 128);
  }
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

bool Client::Put(const std::string& key, const std::string& val) {
  // 0. compute key hash
  auto hash = Hash(key);
  
  // 1. get tablet and server info
  //auto& tablet = index_manager_.GetTablet(hash);
  auto& server = index_manager_.GetServer(hash);

  // 2. post ib send and recv
  auto qp = GetQP(server);
  auto sb = GetBuffer(send_bufs_, server);
  auto r = Request::New(sb->buf, Request::Type::PUT, key, val, hash);
  auto rb = GetBuffer(recv_bufs_, server);
  ib_.PostReceive(qp, rb);
  ib_.PostSendAndWait(qp, sb, r->Len(), &server.ib_addr);
  auto b = ib_.Receive(qp, nullptr);
  return b == rb;
}

 bool Client::Delete(const std::string& key) {
  return false;
}

} // namespace nvds
