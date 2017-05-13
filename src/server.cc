#include "server.h"

#include "config.h"
#include "json.hpp"
#include "request.h"

#include <chrono>
#include <thread>

namespace nvds {

using json = nlohmann::json;

Server::Server(uint16_t port, NVMPtr<NVMDevice> nvm, uint64_t nvm_size)
    : BasicServer(port), id_(0),
      active_(false), nvm_size_(nvm_size), nvm_(nvm),
      send_bufs_(ib_.pd(), kSendBufSize, kMaxIBQueueDepth, false),
      recv_bufs_(ib_.pd(), kRecvBufSize, kMaxIBQueueDepth, true) {
  // Infiniband
  scq_ = ib_.CreateCQ(kMaxIBQueueDepth);
  rcq_ = ib_.CreateCQ(kMaxIBQueueDepth);
  qp_ = new Infiniband::QueuePair(ib_, IBV_QPT_UD, Infiniband::kPort, nullptr,
      scq_, rcq_, kMaxIBQueueDepth, kMaxIBQueueDepth);
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
  // This has a great
  // Fill the receive queue
  for (size_t i = 0; i < kMaxIBQueueDepth; ++i) {
    auto b = recv_bufs_.Alloc();
    assert(b);
    ib_.PostReceive(qp_, b);
  }

  using namespace std::chrono;
  //uint64_t cnt = 0;
  high_resolution_clock::time_point begin;
  while (true) {
    auto b = ib_.TryReceive(qp_);
    if (b != nullptr) {
      // Dispatch to worker's queue, then worker get work from it's queue.
      // The buffer `b` will be freed by the worker. Thus, the buffer pool
      // must made thread safe.
      Dispatch(b);
    }
    if ((b = ib_.TrySend(qp_)) != nullptr) {
      send_bufs_.Free(b);
    }

    // The receive queue is not full, if there are free buffers.
    if ((b = recv_bufs_.Alloc()) != nullptr) {
      assert(b);
      // Try posting receive if we could
      ib_.PostReceive(qp_, b);
    }
    
    // Performance measurement
    /*
    if (cnt == 0) {
      begin = high_resolution_clock::now();
    }
    if (++cnt == 500 * 1000) {
      auto end = high_resolution_clock::now();
      double t = duration_cast<duration<double>>(end - begin).count();
      NVDS_LOG("server %d QPS: %.2f, time: %.2f", id_, cnt / t, t);
      cnt = 0;
    }
    */
  }
}

void Server::Dispatch(Work* work) {
  auto r = work->MakeRequest();
  auto id = index_manager_.GetTabletId(r->key_hash);
  // DEBUG
  //r->Print();
  //std::cout << "server id: " << id_ << std::endl;
  //std::cout << "tablet id: " << id << std::endl;
  //std::cout << std::flush;
  workers_[id % kNumTabletAndBackupsPerServer]->Enqueue(work);
}

void Server::Worker::Serve() {
  while (true) {
    // Get request
    Work* work = nullptr;
    Infiniband::Buffer* sb = nullptr;
    std::unique_lock<std::mutex> lock(mtx_);
    cond_var_.wait(lock, [&work, &sb, this]() -> bool {
        return (work || (work = wq_.Dequeue())) &&
               (sb || (sb = server_->send_bufs_.Alloc()));
      });
    assert(work != nullptr && sb != nullptr);

    // Do the work
    auto r = work->MakeRequest();
    auto resp = Response::New(sb, r->type, Response::Status::OK);
    switch (r->type) {
    case Request::Type::PUT:
      std::cout << "a put request" << std::endl;
      resp->status = tablet_->Put(r);
      resp->Print();
      break;
    case Request::Type::DEL:
      std::cout << "a get request" << std::endl;
      resp->status = tablet_->Del(r);
      resp->Print();      
      break;
    case Request::Type::GET:
      std::cout << "a get request" << std::endl;
      r->Print();
      resp->status = tablet_->Get(resp, r);
      resp->Print();
      break;
    }
    // TODO(wgtdkp): collect nvm writes
    server_->ib_.PostSend(server_->qp_, sb, resp->Len(), &work->peer_addr);
    server_->recv_bufs_.Free(work);
  }
}

} // namespace nvds
