#ifndef _NVDS_SERVER_H_
#define _NVDS_SERVER_H_

#include "basic_server.h"
#include "common.h"
#include "index.h"
#include "infiniband.h"
#include "measurement.h"
#include "message.h"
#include "spinlock.h"
#include "tablet.h"

#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace nvds {

/*
 * The abstraction of NVM device. No instantiation is allowed.
 * For each server, there is only one NVMDevice object which is
 * mapped to the address passed when the server is started.
 */
struct NVMDevice {

  // The server id of the machine.
  ServerId server_id;
  // The size in byte of the NVM device
  uint64_t size;
  // The key hash range
  KeyHash key_begin;
  KeyHash key_end;
  // The number of tablets
  uint32_t tablet_num;
  // The begin of tablets
  NVMTablet tablets[0];
  NVMDevice() = delete;
};

static const uint64_t kNVMDeviceSize = sizeof(NVMDevice) +
                                       kNVMTabletSize * kNumTabletAndBackupsPerServer;

class Server : public BasicServer {
 // For convenience
 public:
  Measurement alloc_measurement;
  Measurement sync_measurement;
  Measurement thread_measurement;
  Measurement send_measurement;
  //Measurement recv_measurement;

 public:
  // Workers & tablets
  friend class Worker;
  using Work = Infiniband::Buffer;
  Server(uint16_t port, NVMPtr<NVMDevice> nvm, uint64_t nvm_size);
  ~Server();
  DISALLOW_COPY_AND_ASSIGN(Server);

  ServerId id() const { return id_; }
  bool active() const { return active_; }  
  uint64_t nvm_size() const { return nvm_size_; }
  NVMPtr<NVMDevice> nvm() const { return nvm_; }
  size_t num_recv() const { return num_recv_; }

  void Run() override;
  bool Join(const std::string& coord_addr);
  void Leave();
  void Listening();
  void Poll();
  // Dispatch the `work` to specific `worker`, load balance considered.
  void Dispatch(Work* work);

 private:
  void HandleRecvMessage(std::shared_ptr<Session> session,
                         std::shared_ptr<Message> msg);
  void HandleSendMessage(std::shared_ptr<Session> session,
                         std::shared_ptr<Message> msg);

  // The WorkQueue will be enqueued by the dispatch,
  // and dequeued by the worker. Thus, it must be made thread safe.
  class WorkQueue {
   public:
    WorkQueue() {}
    DISALLOW_COPY_AND_ASSIGN(WorkQueue);
    // Made Thread safe
    void Enqueue(Work* work) {
      std::lock_guard<Spinlock> _(spinlock_);
      if (head_ == nullptr) {
        //std::lock_guard<Spinlock> _(spinlock_);
        tail_ = work;
        head_ = work;
      } else {
        tail_->next = work;
        tail_ = work;
      }
    }
    // Made Thread safe
    Work* Dequeue() {
      std::lock_guard<Spinlock> _(spinlock_);
      if (head_ == nullptr) {
        return nullptr;
      } else if (head_ == tail_) {
        auto ans = head_;
        tail_ = nullptr;
        head_ = nullptr;
        return const_cast<Work*>(ans);
      } else {
        auto ans = head_;
        head_ = head_->next;
        return const_cast<Work*>(ans);
      }
    }

    Work* TryPollWork() {
      Measurement m;
      m.begin();
      auto ans = Dequeue();
      while (ans == nullptr && m.cur_period() < 200) {
        ans = Dequeue();
      }
      return const_cast<Work*>(ans);
    }

   private:
    volatile Work* head_ = nullptr;
    Work* tail_ = nullptr;
    Spinlock spinlock_;
  };

  class Worker {
   public:
    Worker(Server* server, Tablet* tablet)
        : server_(server), tablet_(tablet),
          slave_(std::bind(&Worker::Serve, this)) {}
    void Enqueue(Work* work) {
      std::unique_lock<std::mutex> lock(mtx_);
      wq_.Enqueue(work);
      #ifdef ENABLE_MEASUREMENT
        server_->thread_measurement.begin();
      #endif
      cond_var_.notify_one();
    }

   private:
    void Serve();
    WorkQueue wq_;
    Server* server_;
    Tablet* tablet_;

    std::mutex mtx_;
    std::condition_variable cond_var_;
    std::thread slave_;
  };

  static const uint32_t kMaxIBQueueDepth = 128;
  ServerId id_;
  bool active_;
  uint64_t nvm_size_;  
  NVMPtr<NVMDevice> nvm_;

  IndexManager index_manager_;

  // Infiniband
  Infiniband ib_;
  Infiniband::Address ib_addr_;
  Infiniband::RegisteredBuffers send_bufs_;
  Infiniband::RegisteredBuffers recv_bufs_;
  Infiniband::QueuePair* qp_;

  // Worker
  std::array<Worker*, kNumTabletsPerServer> workers_;
  std::array<Tablet*, kNumTabletAndBackupsPerServer> tablets_;

  // Statistic
  size_t num_recv_ {0};
};

} // namespace nvds

#endif // _NVDS_SERVER_H_
