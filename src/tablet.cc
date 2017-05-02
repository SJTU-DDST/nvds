#include "tablet.h"

#define OFFSETOF_NVMOBJECT(obj, member) \
    offsetof(NVMObject, member) + obj

namespace nvds {

Tablet::Tablet(const TabletInfo& info, NVMPtr<NVMTablet> nvm_tablet)
    : nvm_tablet_(nvm_tablet), allocator_(&nvm_tablet->data),
      send_bufs_(ib_.pd(), kSendBufSize, kNumReplicas, false),
      recv_bufs_(ib_.pd(), kRecvBufSize, kNumReplicas, true) {
  nvm_tablet_->info = info;
  scq_ = ib_.CreateCQ(1);
  rcq_ = ib_.CreateCQ(1);
  for (size_t i = 0; i < kNumReplicas; ++i) {
    qps_[i] = new Infiniband::QueuePair(ib_, IBV_QPT_RC, Infiniband::kPort,
        nullptr, scq_, rcq_, 2 * kNumReplicas, 2 * kNumReplicas);
    assert(qps_[i] != nullptr);
  }
}

Tablet::~Tablet() {
  for (ssize_t i = kNumReplicas - 1; i >= 0; --i) {
    delete qps_[i];
  }
  ib_.DestroyCQ(rcq_);
  ib_.DestroyCQ(scq_);
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
    allocator_.Write<NVMObject>(obj, {hash, key_len, val_len, next});
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

} // namespace nvds
