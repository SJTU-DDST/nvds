#include "server.h"

#include "config.h"
#include "json.hpp"
#include "request.h"

#include <thread>

namespace nvds {

using json = nlohmann::json;

Server::Server(NVMPtr<NVMDevice> nvm, uint64_t nvm_size)
    : BasicServer(kServerPort), id_(0),
      active_(false), nvm_size_(nvm_size), nvm_(nvm),
      send_bufs_(ib_.pd(), kSendBufSize, kNumTabletsPerServer, false),
      recv_bufs_(ib_.pd(), kRecvBufSize, kNumTabletsPerServer, true) {
  // Infiniband
  scq_ = ib_.CreateCQ(kNumTabletsPerServer);
  rcq_ = ib_.CreateCQ(kNumTabletsPerServer);
  qp_ = new Infiniband::QueuePair(ib_, IBV_QPT_UD, Infiniband::kPort, nullptr,
      scq_, rcq_, kNumTabletsPerServer, kNumTabletsPerServer);
  qp_->Activate();
  
  ib_addr_ = {
    Infiniband::kPort,
    ib_.GetLid(Infiniband::kPort),
    qp_->GetLocalQPNum()
  };

  // Tablets
  for (uint32_t i = 0; i < kNumTabletAndBackupsPerServer; ++i) {
    auto ptr = reinterpret_cast<char*>(&nvm_->tablets) + i * kNVMTabletSize;
    bool is_backup = i >= kNumTabletsPerServer;
    tablets_[i] = new Tablet(NVMPtr<NVMTablet>(reinterpret_cast<NVMTablet*>(ptr)),
                             is_backup);
    if (i < kNumTabletsPerServer) {
      workers_[i] = new Worker(this, tablets_[i]);
    }
  }
}

Server::~Server() {
  // Destruct elements in reverse order
  for (int64_t i = kNumTabletAndBackupsPerServer - 1; i >= 0; --i) {
    delete tablets_[i];
    if (i < kNumTabletsPerServer) {
      delete workers_[i];
    }
  }
  delete qp_;
  ib_.DestroyCQ(scq_);
  ib_.DestroyCQ(rcq_);
}

void Server::Run() {
  std::thread poller(&Server::Poll, this);
  
  Accept(std::bind(&Server::HandleRecvMessage, this,
                   std::placeholders::_1, std::placeholders::_2),
         std::bind(&Server::HandleSendMessage, this,
                   std::placeholders::_1, std::placeholders::_2));
  RunService();
  
  poller.join();
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

    std::vector<Infiniband::QueuePairInfo> qpis;
    for (uint32_t i = 0; i < kNumTabletAndBackupsPerServer; ++i) {
      for (uint32_t j = 0; j < kNumReplicas; ++j) {
        qpis.emplace_back(tablets_[i]->info().qpis[j]);
      }
    }
    json body {
      {"size", nvm_size_},
      {"ib_addr", ib_addr_},
      {"tablet_qpis", qpis},
    };
    Message msg(header, body.dump());

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

      auto& server_info = index_manager_.GetServer(id_);
      for (uint32_t i = 0; i < kNumTabletAndBackupsPerServer; ++i) {
        auto tablet_id = server_info.tablets[i];
        // Both master and backup tablets
        tablets_[i]->SettingupQPConnect(tablet_id, index_manager_);
      }
    } catch (boost::system::system_error& err) {
      NVDS_ERR("receive join response from coordinator failed: %s",
               err.what());
      return false;
    }
  } catch (boost::system::system_error& err) {
    NVDS_ERR("connect to coordinator: %s: %" PRIu16 " failed",
             Config::coord_addr().c_str(), kCoordPort);
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

void Server::Poll() {
  // Post 3 extra Receives
  for (size_t i = 0; i < 3; ++i) {
    ib_.PostReceive(qp_, recv_bufs_.Alloc());
  }
  while (true) {
    auto b = ib_.Receive(qp_);
    
    // Dispatch(b, peer_addr);
    // Dispatch to worker's queue, then worker get work from it's queue.
    // The buffer `b` will be freed by the worker, thus, the buffer pool
    // must made thread safe.
    Dispatch(b);

    // recv_bufs_.Alloc() is fast, don't worry.
    if ((b = recv_bufs_.Alloc()) != nullptr) {
      ib_.PostReceive(qp_, b);
    }
  }
}

void Server::Dispatch(Work* work) {
  auto r = work->MakeRequest();
  assert(r->Len() == work->msg_len);
  auto id = index_manager_.GetTabletId(r->key_hash);
  workers_[id]->Enqueue(work);
}

void Server::Worker::Serve() {
  while (true) {
    // Get request
    Work* work;
    std::unique_lock<std::mutex> lock(mtx_);
    cond_var_.wait(lock, [&work, this]() -> bool {
        return (work = wq_.Dequeue()); });
    assert(work != nullptr);

    // Serve
    auto r = work->MakeRequest();
    Infiniband::Buffer* b;
    while ((b = server_->send_bufs_.Alloc()) == nullptr) {}

    switch (r->type) {
    case Request::Type::PUT:
      tablet_->Put(r->key_hash, r->key_len, r->Key(),r->val_len, r->Val());
      break;
    case Request::Type::GET:
      tablet_->Get(b->buf, r->key_hash, r->key_len, r->Key());
      break;
    case Request::Type::DEL:
      tablet_->Del(r->key_hash, r->key_len, r->Key());
      break;
    }

    // TODO(wgtdkp): Put response

  }
}

} // namespace nvds
