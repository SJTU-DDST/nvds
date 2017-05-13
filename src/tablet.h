#ifndef _NVDS_TABLET_H_
#define _NVDS_TABLET_H_

#include "allocator.h"
#include "common.h"
#include "hash.h"
#include "message.h"
#include "response.h"

namespace nvds {

struct Request;
class IndexManager;

// A reasonable prime number
static const uint32_t kHashTableSize = 99871;

struct NVMObject {
  uint32_t next;
  uint16_t key_len;
  uint16_t val_len;
  KeyHash key_hash;    
  char data[0];
};

struct NVMTablet {
  char data[Allocator::kSize];
  std::array<uint32_t, kHashTableSize> hash_table;
  NVMTablet() {
    hash_table.fill(0);
  }
};
static const uint32_t kNVMTabletSize = sizeof(NVMTablet);

class Tablet {
 public:
  //Tablet(const TabletInfo& info, NVMPtr<NVMTablet> nvm_tablet);
  Tablet(NVMPtr<NVMTablet> nvm_tablet, bool is_backup=false);
  ~Tablet();
  DISALLOW_COPY_AND_ASSIGN(Tablet);

  const TabletInfo& info() const { return info_; }

  Response::Status Get(Response* resp, const Request* r);
  Response::Status Del(const Request* r);
  Response::Status Put(const Request* r);
  void SettingupQPConnect(TabletId id, const IndexManager& index_manager);

 private:
  TabletInfo info_;
  NVMPtr<NVMTablet> nvm_tablet_;
  Allocator allocator_;

  // Infiniband
  Infiniband ib_;
  //Infiniband::Address ib_addr_;
  ibv_mr* mr_;
  Infiniband::RegisteredBuffers send_bufs_;
  Infiniband::RegisteredBuffers recv_bufs_;
  std::array<Infiniband::QueuePair*, kNumReplicas> qps_;
  // `kNumReplica` queue pairs share this `rcq_` and `scq_`
  ibv_cq* rcq_;
  ibv_cq* scq_;
};

} // namespace nvds

#endif // _NVDS_TABLET_H_
