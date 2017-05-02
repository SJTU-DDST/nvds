#ifndef _NVDS_TABLET_H_
#define _NVDS_TABLET_H_

#include "allocator.h"
#include "common.h"
#include "hash.h"
#include "message.h"

namespace nvds {

// A reasonable prime number
static const uint32_t kHashTableSize = 99871;

struct NVMObject {
  KeyHash key_hash;
  uint16_t key_len;
  uint16_t val_len;
  uint32_t next;
  char data[0];
};

struct NVMTablet {
  TabletInfo info;
  std::array<uint32_t, kHashTableSize> hash_table;
  char data[0];
  NVMTablet() {
    hash_table.fill(0);
  }
};
static const uint32_t kNVMTabletSize = sizeof(NVMTablet) + Allocator::kSize;


class Tablet {
 public:
  Tablet(const TabletInfo& info, NVMPtr<NVMTablet> nvm_tablet);
  ~Tablet();
  DISALLOW_COPY_AND_ASSIGN(Tablet);

  const TabletInfo& info() const { return nvm_tablet_->info; }
  // TODO(wgtdkp): Directly return NVM address of the value
  // Find the key and copy the value into `val`.
  // Return: -1, not found; else, the length of the `val`.
  int32_t Get(char* val, KeyHash hash, uint16_t key_len, const char* key);
  void    Del(KeyHash hash, uint16_t key_len, const char* key);
  // Return: -1, error(no enough space);
  int32_t Put(KeyHash hash, uint16_t key_len, const char* key,
              uint16_t val_len, const char* val);
  
 private:
  NVMPtr<NVMTablet> nvm_tablet_;
  Allocator allocator_;

  // Infiniband
  Infiniband ib_;
  //Infiniband::Address ib_addr_;
  Infiniband::RegisteredBuffers send_bufs_;
  Infiniband::RegisteredBuffers recv_bufs_;
  std::array<Infiniband::QueuePair*, kNumReplicas> qps_;
  // `kNumReplica` queue pairs share this `rcq_` and `scq_`
  ibv_cq* rcq_;
  ibv_cq* scq_;
};

} // namespace nvds

#endif // _NVDS_TABLET_H_
