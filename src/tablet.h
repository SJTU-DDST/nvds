#ifndef _NVDS_TABLET_H_
#define _NVDS_TABLET_H_

#include "common.h"
#include "hash.h"

namespace nvds {

/*
PACKED(
struct NVMTablet {
  // Tablet id, unique in scope of node.
  // There could be same tablet id on different node.
  TabletId id;
  PACKED(
  struct Backup {
    ServerId server_id;
    TabletId tablet_id;
  });
  // The backups of this tablet.
  Backup backups[kNumReplica];
  
});
*/

struct Backup {
  ServerId server_id;
  TabletId tablet_id;
};
using Master = Backup;

struct TabletInfo {
  TabletId id;
  bool is_backup;
  union {
    // If `is_backup_` == true,
    // `master_` is the master tablet of this backup tablet
    Master master;
    // Else, `backups_` is the backups of this makster tablet
    Backup backups[kNumReplicas];
  };
};

class Tablet {
 public:
  Tablet() {}
  ~Tablet() {}
  DISALLOW_COPY_AND_ASSIGN(Tablet);
  
  const TabletInfo& info() const { return info_; }

 private:
  TabletInfo info_;
};

} // namespace nvds

#endif // _NVDS_TABLET_H_
