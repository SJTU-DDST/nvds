#include "tablet.h"

#include "index.h"
#include "request.h"

#define OFFSETOF_NVMOBJECT(obj, member) \
    offsetof(NVMObject, member) + obj

namespace nvds {

Tablet::Tablet(NVMPtr<NVMTablet> nvm_tablet, bool is_backup)
    : nvm_tablet_(nvm_tablet), allocator_(&nvm_tablet->data),
      send_bufs_(ib_.pd(), kSendBufSize, kNumReplicas, false),
      recv_bufs_(ib_.pd(), kRecvBufSize, kNumReplicas, true) {
  info_.is_backup = is_backup;
  
  // Memory region
  // FIXME(wgtdkp): how to simulate latency of RDMA read/write to NVM?
  mr_ = ibv_reg_mr(ib_.pd().pd(), nvm_tablet_.ptr(), kNVMTabletSize,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  assert(mr_ != nullptr);

  scq_ = ib_.CreateCQ(1);
  rcq_ = ib_.CreateCQ(1);
  for (size_t i = 0; i < kNumReplicas; ++i) {
    qps_[i] = new Infiniband::QueuePair(ib_, IBV_QPT_RC, Infiniband::kPort,
        nullptr, scq_, rcq_, 2 * kNumReplicas, 2 * kNumReplicas);
    info_.qpis[i] = {
      ib_.GetLid(Infiniband::kPort),
      qps_[i]->GetLocalQPNum(),
      kQPPsn,
      mr_->rkey,
      reinterpret_cast<uint64_t>(nvm_tablet_.ptr())
    };
  }
}

Tablet::~Tablet() {
  for (ssize_t i = kNumReplicas - 1; i >= 0; --i) {
    delete qps_[i];
  }
  ib_.DestroyCQ(rcq_);
  ib_.DestroyCQ(scq_);
  ibv_dereg_mr(mr_);  
}

// FIXME(wgtdkp): Remove the item if there already exists
int32_t Tablet::Put(KeyHash hash, uint16_t key_len, const char* key,
                    uint16_t val_len, const char* val) {
  auto idx = hash % kHashTableSize;
  auto head = nvm_tablet_->hash_table[idx];
  auto q = head;
  auto p = q == 0 ? 0 : allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(head, next));
  while (p) {
    if (key_len != allocator_.Read<uint16_t>(p + offsetof(NVMObject, key_len)) ||
        allocator_.Memcmp(OFFSETOF_NVMOBJECT(p, data), key, key_len)) {
      q = p;
      p = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
      continue;
    }
    if (val_len < allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, val_len))) {
      // The new value is shorter than the older, store data
      // at its original place.
      allocator_.Write(OFFSETOF_NVMOBJECT(p, val_len), val_len);
      allocator_.Memcpy(OFFSETOF_NVMOBJECT(p, data) + key_len, val, val_len);
    } else {
      auto next = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
      auto size = Allocator::RoundupBlockSize(sizeof(NVMObject) + key_len + val_len);
      allocator_.FreeBlock(p);
      p = allocator_.AllocBlock(size);
      allocator_.Write(OFFSETOF_NVMOBJECT(q, next), p);
      allocator_.Write(OFFSETOF_NVMOBJECT(p, next), next);
    }
    break;
  }
  if (!p) {
    auto next = head == 0 ? 0 : allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(head, next));
    auto size = Allocator::RoundupBlockSize(sizeof(NVMObject) + key_len + val_len);
    auto obj = allocator_.AllocBlock(size);
    allocator_.Write<NVMObject>(obj, {next, key_len, val_len, next});
    // TODO(wgtdkp): use single `memcpy`
    allocator_.Memcpy(OFFSETOF_NVMOBJECT(obj, data), key, key_len);
    allocator_.Memcpy(OFFSETOF_NVMOBJECT(obj, data) + key_len, val, val_len);
    allocator_.Write(head, obj);
  }
  return 0;
}

int32_t Tablet::Get(char* val, KeyHash hash,
                    uint16_t key_len, const char* key) {
  auto idx = hash % kHashTableSize;
  auto p = nvm_tablet_->hash_table[idx];
  if (!p) return -1;
  p = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
  while (p) {
    // TODO(wgtdkp): use a different hash to speedup searching
    if (allocator_.Read<uint16_t>(p + offsetof(NVMObject, key_len)) != key_len ||
        allocator_.Memcmp(OFFSETOF_NVMOBJECT(p, data), key, key_len)) {
      // FIXME(wgtdkp): use nvmcpy instead.
      auto val_len = allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, val_len));
      allocator_.Memcpy(val, OFFSETOF_NVMOBJECT(p, data) + key_len, val_len);
      return val_len;
    }
    p = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
  }
  return -1;
}

void Tablet::Del(KeyHash hash, uint16_t key_len, const char* key) {
  auto idx = hash % kHashTableSize;
  auto q = nvm_tablet_->hash_table[idx];
  if (!q) return;
  auto p = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(q, next));
  while (p) {
    if (allocator_.Read<uint16_t>(p + offsetof(NVMObject, key_len)) != key_len ||
        allocator_.Memcmp(OFFSETOF_NVMOBJECT(p, data), key, key_len)) {
      auto next = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
      allocator_.Write(OFFSETOF_NVMOBJECT(q, next), next);
      allocator_.FreeBlock(p);
      return;
    }
  }
}

void Tablet::Serve(Request& r) {
  switch (r.type) {
  case Request::Type::PUT:
    break;
  case Request::Type::GET:
    break;
  case Request::Type::DEL:
    break;
  }

}

void Tablet::SettingupQPConnect(TabletId id, const IndexManager& index_manager) {
  info_ = index_manager.GetTablet(id);
  if (info_.is_backup) {
    auto master_info = index_manager.GetTablet(info_.master);
    assert(!master_info.is_backup);
    for (uint32_t i = 0; i < kNumReplicas; ++i) {
      if (master_info.backups[i] == id) {
        qps_[0]->Plumb(master_info.qpis[i]);
        break;
      }
      assert(i < kNumReplicas - 1);
    }
  } else {
    for (uint32_t i = 0; i < kNumReplicas; ++i) {
      auto backup_id = info_.backups[i];
      auto backup_info = index_manager.GetTablet(backup_id);
      if (!backup_info.is_backup) {
        NVDS_LOG("tablet id: %d", id);
        NVDS_LOG("tablet backup id: %d", backup_id);
      }
      assert(backup_info.is_backup);
      qps_[i]->Plumb(backup_info.qpis[0]);
    }
  }
}

} // namespace nvds
