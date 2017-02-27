#ifndef _NVDS_TABLET_H_
#define _NVDS_TABLET_H_

#include "common.h"

namespace nvds {

PACKED(
struct NVMTablet {
  // Tablet id, unique in scope of node.
  // There could be same tablet id on different node.
  uint32_t id;
  PACKED(
  struct Backup {
    uint32_t server_id;
    uint32_t tablet_id;
  });
  // The backups of this tablet.
  Backup backups[kNumReplica];
  
});

class Tablet {
 public:
  Tablet(KeyHash begin, KeyHash end)
      : begin_(begin), end_(end) {}
  ~Tablet() {}
  DISALLOW_COPY_AND_ASSIGN(Tablet);

  KeyHash begin() const { return begin_; }
  KeyHash end() const { return end_; }

 private:
  // The hash key begin of the tablet
  KeyHash begin_;
  // The hash key end of the tablet
  KeyHash end_;
};

} // namespace nvds

#endif // _NVDS_TABLET_H_
