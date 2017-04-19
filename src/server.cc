#include "server.h"

#include "config.h"
#include "json.hpp"

namespace nvds {

using json = nlohmann::json;

Server::Server(NVMPtr<NVMDevice> nvm, uint64_t nvm_size)
    : BasicServer(kServerPort), id_(0),
      active_(false), nvm_size_(nvm_size), nvm_(nvm),
      send_bufs_(ib_.pd(), kSendBufSize, kNumTabletsPerServer),
      recv_bufs_(ib_.pd(), kRecvBufSize, kNumTabletsPerServer, true) {
  int32_t ib_port = 1;
  scq_ = ib_.CreateCQ(kNumTabletsPerServer);
  rcq_ = ib_.CreateCQ(kNumTabletsPerServer);
  qp_ = new Infiniband::QueuePair(ib_, IBV_QPT_RC, ib_port, nullptr,
      scq_, rcq_, kNumTabletsPerServer, kNumTabletsPerServer);
  ib_addr_ = {ib_port, ib_.GetLid(ib_port), qp_->GetLocalQPNum()};
}

void Server::Run() {
  Accept(std::bind(&Server::HandleRecvMessage, this,
                   std::placeholders::_1, std::placeholders::_2),
         std::bind(&Server::HandleSendMessage, this,
                   std::placeholders::_1, std::placeholders::_2));
  RunService();
}

bool Server::Join() {
  using boost::asio::ip::tcp;
  tcp::resolver resolver(tcp_service_);
  tcp::resolver::query query(Config::coord_addr(), std::to_string(kCoordPort));
  auto ep = resolver.resolve(query);
  try {
    boost::asio::connect(conn_sock_, ep);
    Session session_join(std::move(conn_sock_));

    Message::Header header {Message::SenderType::SERVER,
                            Message::Type::REQ_JOIN, 0};
    Message msg(header,
                json({{"size", nvm_size_}, {"ib_addr", ib_addr_}}).dump());

    try {
      session_join.SendMessage(msg);
    } catch (boost::system::system_error& err) {
      NVDS_ERR("send join request to coordinator failed: %s", err.what());
      return false;
    }
    try {
      msg = session_join.RecvMessage();
      assert(msg.sender_type() == Message::SenderType::COORDINATOR);
      assert(msg.type() == Message::Type::RES_JOIN);
      
      auto j_body = json::parse(msg.body());
      id_ = j_body["id"];
      index_manager_ = j_body["index_manager"];
      active_ = true;
    } catch (boost::system::system_error& err) {
      NVDS_ERR("receive join response from coordinator failed: %s",
               err.what());
      return false;
    }
  } catch (boost::system::system_error& err) {
    NVDS_ERR("connect to coordinator: %s: %" PRIu16 "failed: %s",
             Config::coord_addr().c_str(), kCoordPort, err.what());
    return false;
  }
  return true;
}

void Server::Leave() {
  assert(false);
}

void Server::Listening() {
  assert(false);
}

void Server::HandleRecvMessage(std::shared_ptr<Session> session,
                               std::shared_ptr<Message> msg) {
  assert(false);
}

void Server::HandleSendMessage(std::shared_ptr<Session> session,
                               std::shared_ptr<Message> msg) {
  assert(false);
}

} // namespace nvds
