#include "client.h"

#include "request.h"
#include "response.h"

namespace nvds {

using json = nlohmann::json;

Client::Client(const std::string& coord_addr)
    : session_(Connect(coord_addr)),
      send_bufs_(ib_.pd(), kSendBufSize, kMaxIBQueueDepth, false),
      recv_bufs_(ib_.pd(), kRecvBufSize, kMaxIBQueueDepth, true) {
  InitIB();
  Join();
}

Client::~Client() {
  Close();
  delete qp_;  
  ib_.DestroyCQ(scq_);
  ib_.DestroyCQ(rcq_);
}

void Client::InitIB() {
  scq_ = ib_.CreateCQ(kMaxIBQueueDepth);
  rcq_ = ib_.CreateCQ(kMaxIBQueueDepth);
  qp_ = new Infiniband::QueuePair(ib_, IBV_QPT_UD, Infiniband::kPort,
      nullptr, scq_, rcq_, kMaxIBQueueDepth, kMaxIBQueueDepth);
  qp_->Activate();
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

std::string Client::Get(const std::string& key) {
  // 0. compute key hash
  auto hash = Hash(key);
  
  // 1. get tablet and server info
  //auto& tablet = index_manager_.GetTablet(hash);
  auto& server = index_manager_.GetServer(hash);
  // 2. post ib send and recv
  auto sb = send_bufs_.Alloc();
  assert(sb != nullptr);
  auto r = Request::New(sb, Request::Type::GET, key, "", hash);
  auto rb = recv_bufs_.Alloc();
  assert(rb != nullptr);
  ib_.PostReceive(qp_, rb);
  ib_.PostSendAndWait(qp_, sb, r->Len(), &server.ib_addr);
  Request::Del(r);
  send_bufs_.Free(sb);
  assert(rb == ib_.Receive(qp_));
  auto resp = rb->MakeResponse();
  //resp->Print();
  std::string ans(resp->val, resp->val_len);
  recv_bufs_.Free(rb);
  return ans;
}

bool Client::Put(const std::string& key, const std::string& val) {
  // 0. compute key hash
  auto hash = Hash(key);
  
  // 1. get tablet and server info
  //auto& tablet = index_manager_.GetTablet(hash);
  auto& server = index_manager_.GetServer(hash);
  // 2. post ib send and recv
  auto sb = send_bufs_.Alloc();
  assert(sb != nullptr);
  auto r = Request::New(sb, Request::Type::PUT, key, val, hash);
  auto rb = recv_bufs_.Alloc();
  assert(rb != nullptr);
  ib_.PostReceive(qp_, rb);
  ib_.PostSendAndWait(qp_, sb, r->Len(), &server.ib_addr);
  Request::Del(r);
  send_bufs_.Free(sb);
  assert(rb == ib_.Receive(qp_));
  auto resp = rb->MakeResponse();
  //resp->Print();
  bool ans = resp->status == Response::Status::OK;
  recv_bufs_.Free(rb);
  return ans;
}

bool Client::Del(const std::string& key) {
  // 0. compute key hash
  auto hash = Hash(key);

  // 1. get tablet and server info
  //auto& tablet = index_manager_.GetTablet(hash);
  auto& server = index_manager_.GetServer(hash);
  // 2. post ib send and recv
  auto sb = send_bufs_.Alloc();
  assert(sb != nullptr);
  auto r = Request::New(sb, Request::Type::DEL, key, "", hash);
  auto rb = recv_bufs_.Alloc();
  assert(rb != nullptr);
  ib_.PostReceive(qp_, rb);
  ib_.PostSendAndWait(qp_, sb, r->Len(), &server.ib_addr);
  Request::Del(r);
  send_bufs_.Free(sb);
  assert(rb == ib_.Receive(qp_));
  auto resp = rb->MakeResponse();
  //resp->Print();
  bool ans = resp->status == Response::Status::OK;
  recv_bufs_.Free(rb);
  return ans;
}

} // namespace nvds
