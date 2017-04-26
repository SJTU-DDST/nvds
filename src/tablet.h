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
  NVMPtr<NVMObject> next;
  KeyHash key_hash;
  uint16_t key_len;
  uint16_t val_len;
  char data[0];
};

struct NVMTablet {
  TabletInfo info;
  std::array<NVMPtr<NVMObject>, kHashTableSize> hash_table;
  char data[0];
};
static const uint32_t kNVMTabletSize = sizeof(NVMTablet) + Allocator::kSize;


class Tablet {
 public:
  
  Tablet(const TabletInfo& info, NVMPtr<NVMTablet> nvm_tablet)
      : nvm_tablet_(nvm_tablet), allocator_(&nvm_tablet->data) {
    nvm_tablet_->info = info;
  }
  ~Tablet() {}
  DISALLOW_COPY_AND_ASSIGN(Tablet);
  
  const TabletInfo& info() const { return nvm_tablet_->info; }
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

  // TODO(wgtdkp): Locks
};

} // namespace nvds

#endif // _NVDS_TABLET_H_
