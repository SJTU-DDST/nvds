#include "server.h"

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
  qp_ = new Infiniband::QueuePair(ib_, IBV_QPT_UD,
      kMaxIBQueueDepth, kMaxIBQueueDepth);
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
    tablets_[i] = new Tablet(index_manager_,
        NVMPtr<NVMTablet>(reinterpret_cast<NVMTablet*>(ptr)), is_backup);
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

bool Server::Join(const std::string& coord_addr) {
  using boost::asio::ip::tcp;
  tcp::resolver resolver(tcp_service_);
  tcp::resolver::query query(coord_addr, std::to_string(kCoordPort));
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
             coord_addr.c_str(), kCoordPort);
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

#if 0 //#ifdef ENABLE_MEASUREMENT
  while (true) {
    Infiniband::Buffer* b = nullptr;
    //std::clog << "1" << std::endl;
    while ((b = ib_.TryReceive(qp_)) == nullptr) {}
    //std::clog << "2" << std::endl;    
    static bool enable = false;
    if (enable) {
      recv_measurement.end();
    }
    enable = true;
    Dispatch(b);
    //std::clog << "3" << std::endl;

    while ((b = ib_.TrySend(qp_)) == nullptr) {}
    //std::clog << "4" << std::endl;
    send_measurement.end();
    recv_measurement.begin();
    send_bufs_.Free(b);

    b = recv_bufs_.Alloc();
    assert(b != nullptr);
    ib_.PostReceive(qp_, b);
  }
#else
  while (true) {
    auto b = ib_.TryReceive(qp_);
    if (b != nullptr) {
      #ifdef ENABLE_MEASUREMENT
        static bool enable = false;
        // For evaluating GET latency
        if (alloc_measurement.num_repetitions() == 10000) {
          alloc_measurement.Reset();
          sync_measurement.Reset();
          thread_measurement.Reset();
          send_measurement.Reset();
          enable = false;
        }
        // For evaluating GET latency        
        if (enable) {
          //recv_measurement.end();
          send_measurement.end();
        }
        enable = true;
      #endif
      // Dispatch to worker's queue, then worker get work from it's queue.
      // The buffer `b` will be freed by the worker. Thus, the buffer pool
      // must made thread safe.
      Dispatch(b);
    }
    if ((b = ib_.TrySend(qp_)) != nullptr) {
      #ifdef ENABLE_MEASUREMENT
        //send_measurement.end();
        //recv_measurement.begin();
      #endif
      send_bufs_.Free(b);
    }

    // The receive queue is not full, if there are free buffers.
    if ((b = recv_bufs_.Alloc()) != nullptr) {
      assert(b);
      // Try posting receive if we could
      ib_.PostReceive(qp_, b);
    }
  }
#endif
}

void Server::Dispatch(Work* work) {
  auto r = work->MakeRequest();
  auto id = index_manager_.GetTabletId(r->key_hash);
  workers_[id % kNumTabletAndBackupsPerServer]->Enqueue(work);
  ++num_recv_;
}

void Server::Worker::Serve() {
  ModificationList modifications;
  while (true) {
    // Get request
    auto work = wq_.TryPollWork();
    Infiniband::Buffer* sb = nullptr;
    if (work == nullptr) {
      std::unique_lock<std::mutex> lock(mtx_);
      cond_var_.wait(lock, [&work, &sb, this]() -> bool {
          return (work || (work = wq_.Dequeue()));
        });
    }
    #ifdef ENABLE_MEASUREMENT
      server_->thread_measurement.end();
    #endif
    sb = server_->send_bufs_.Alloc();
    assert(work != nullptr && sb != nullptr);

    // Do the work
    auto r = work->MakeRequest();
    auto resp = Response::New(sb, r->type, Status::OK);
    modifications.clear();
    
    #ifdef ENABLE_MEASUREMENT
      server_->alloc_measurement.begin();
    #endif
    switch (r->type) {
    case Request::Type::PUT:
      resp->status = tablet_->Put(r, modifications);
      break;
    case Request::Type::ADD:
      resp->status = tablet_->Add(r, modifications);
      break;
    case Request::Type::DEL:
      resp->status = tablet_->Del(r, modifications);
      break;
    case Request::Type::GET:
      resp->status = tablet_->Get(resp, r, modifications);
      break;
    }
    #ifdef ENABLE_MEASUREMENT
      server_->alloc_measurement.end();
    #endif

    try {
      #ifdef ENABLE_MEASUREMENT
        server_->sync_measurement.begin();
      #endif
      tablet_->Sync(modifications);
      #ifdef ENABLE_MEASUREMENT
        server_->sync_measurement.end();
      #endif
    } catch (TransportException& e) {
      //r->Print();
      //tablet_->info().Print();
      //server_->index_manager_.PrintTablets();
      NVDS_ERR(e.ToString().c_str());
      throw e;
    }

    #ifdef ENABLE_MEASUREMENT
      server_->send_measurement.begin();
    #endif
    server_->ib_.PostSend(server_->qp_, sb, resp->Len(), &work->peer_addr);
    server_->recv_bufs_.Free(work);
  }
}

} // namespace nvds
