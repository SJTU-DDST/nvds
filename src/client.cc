#include "client.h"

#include "request.h"
#include "response.h"

namespace nvds {

using json = nlohmann::json;

Client::Client(const std::string& coord_addr)
    : session_(Connect(coord_addr)),
      send_bufs_(ib_.pd(), kSendBufSize, kMaxIBQueueDepth, false),
      recv_bufs_(ib_.pd(), kRecvBufSize, kMaxIBQueueDepth, true) {
  // Infiniband
  qp_ = new Infiniband::QueuePair(ib_, IBV_QPT_UD,
      kMaxIBQueueDepth, kMaxIBQueueDepth);
  qp_->Activate();
  
  Join();

  for (size_t i = 0; i < kMaxIBQueueDepth / 2; ++i) {
    auto rb = recv_bufs_.Alloc();
    assert(rb);
    ib_.PostReceive(qp_, rb);
  }
}

Client::~Client() {
  Close();
  delete qp_;  
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
  tcp::resolver resolver {tcp_service_};
  tcp::socket conn_sock {tcp_service_};
  tcp::resolver::query query {coord_addr, std::to_string(kCoordPort)};
  auto ep = resolver.resolve(query);
  boost::asio::connect(conn_sock, ep);
  return conn_sock;
}

Client::Buffer* Client::RequestAndWait(const char* key, size_t key_len,
    const char* val, size_t val_len, Request::Type type) {
  assert(key_len + val_len <= kMaxItemSize);
  // 0. compute key hash
  auto hash = Hash(key, key_len);

  // 1. get tablet and server info
  //auto& tablet = index_manager_.GetTablet(hash);
  auto& server = index_manager_.GetServer(hash);
  // 2. post ib send and recv
  auto sb = send_bufs_.Alloc();
  assert(sb != nullptr);
  auto r = Request::New(sb, type, key, key_len, val, val_len, hash);
  
  ib_.PostSendAndWait(qp_, sb, r->Len(), &server.ib_addr);
  Request::Del(r);
  send_bufs_.Free(sb);

  auto ans = ib_.Receive(qp_);
  auto rb = recv_bufs_.Alloc();
  assert(rb != nullptr);
  ib_.PostReceive(qp_, rb);
  return ans;
  //std::string ans(resp->val, resp->val_len);
  //recv_bufs_.Free(rb);
  //return ans;
}

} // namespace nvds
