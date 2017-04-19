#ifndef _NVDS_TABLET_H_
#define _NVDS_TABLET_H_

#include "allocator.h"
#include "common.h"
#include "hash.h"
#include "message.h"

namespace nvds {

static const uint32_t kHashTableSize = 1;

struct NVMObject {
  NVMPtr<NVMObject> next;
  uint16_t key_len;
  uint16_t val_len;
  KeyHash key_hash;
  char data[0];
};

struct NVMTablet {
  TabletInfo info;
  std::array<NVMPtr<NVMObject>, kHashTableSize> hash_table;
  char data[0];
};

class Tablet {
 public:
  Tablet(const TabletInfo& info, NVMPtr<NVMTablet> nvm_tablet)
      : nvm_tablet_(nvm_tablet), allocator_(&nvm_tablet->data) {}
  ~Tablet() {}
  DISALLOW_COPY_AND_ASSIGN(Tablet);
  
  const TabletInfo& info() const { return nvm_tablet_->info; }

 private:
  NVMPtr<NVMTablet> nvm_tablet_;
  Allocator allocator_;
};

} // namespace nvds

#endif // _NVDS_TABLET_H_
