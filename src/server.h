#ifndef _NVDS_SERVER_H_
#define _NVDS_SERVER_H_

#include "basic_server.h"
#include "common.h"
#include "index.h"
#include "infiniband.h"
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
                                       kNVMTabletSize * kNumTabletsPerServer;

class Server : public BasicServer {
 public:
  // Workers & tablets
  using Work = Infiniband::Buffer;
  Server(NVMPtr<NVMDevice> nvm, uint64_t nvm_size);
  ~Server();
  DISALLOW_COPY_AND_ASSIGN(Server);

  ServerId id() const { return id_; }
  bool active() const { return active_; }  
  uint64_t nvm_size() const { return nvm_size_; }
  NVMPtr<NVMDevice> nvm() const { return nvm_; }

  void Run() override;
  bool Join();
  void Leave();
  void Listening();
  void Poll();
  // Dispatch the `work` to specific `worker`, load balance considered.
  void Dispatch(Work* work);

 private:
  static const uint32_t kSendBufSize = 1024 + 128;
  static const uint32_t kRecvBufSize = 1024 * 2 + 128;
  void HandleRecvMessage(std::shared_ptr<Session> session,
                         std::shared_ptr<Message> msg);
  void HandleSendMessage(std::shared_ptr<Session> session,
                         std::shared_ptr<Message> msg);
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
  ibv_cq* rcq_;
  ibv_cq* scq_;

  void InitTabletsAndWorkers();
  // The WorkQueue will be enqueued by the dispatch,
  // and dequeued by the worker. Thus, it must be made thread safe.
  class WorkQueue {
   public:
    WorkQueue() {}
    DISALLOW_COPY_AND_ASSIGN(WorkQueue);
    // Made Thread safe
    void Enqueue(Work* work) {
      if (head == nullptr) {
        std::lock_guard<Spinlock> _(spinlock_);
        tail = work;
        head = work;
      } else if (head == tail) {
        std::lock_guard<Spinlock> _(spinlock_);
        tail->next = work;
        tail = work;
      } else {
        tail->next = work;
        tail = work;
      }
    }
    // Made Thread safe
    Work* Dequeue() {
      if (head == nullptr) {
        return nullptr;
      } else if (head == tail) {
        std::lock_guard<Spinlock> _(spinlock_);
        auto ans = head;
        head = nullptr;
        tail = nullptr;
        return ans;
      } else {
        auto ans = head;
        head = head->next;
        return ans;
      }
    }

   private:
    Work* head = nullptr;
    Work* tail = nullptr;
    Spinlock spinlock_;
  };
  class Worker {
   public:
    Worker(Tablet* tablet)
        : tablet_(tablet), slave_(std::bind(&Worker::Serve, this)) {}
    
   private:
    void Serve() {
      while (true) {
        Work* work;
        std::unique_lock<std::mutex> lock(mtx_);
        cond_var_.wait(lock, [&work, this]() {
            return (work = wq_.Dequeue()) == nullptr; });
        assert(work != nullptr);

        // TODO(wgtdkp): serve request

      }
    }

   private:
    WorkQueue wq_;
    Tablet* tablet_ = nullptr;

    std::mutex mtx_;
    std::condition_variable cond_var_;
    std::thread slave_;
  };

  std::array<Worker*, kNumTabletsPerServer> workers_;
  std::array<Tablet*, kNumTabletsPerServer> tablets_;
};

} // namespace nvds

#endif // _NVDS_SERVER_H_
