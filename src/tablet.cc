#include "tablet.h"

#include "index.h"
#include "request.h"

#define OFFSETOF_NVMOBJECT(obj, member) \
    offsetof(NVMObject, member) + obj

namespace nvds {

using Status = Response::Status;

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

Status Tablet::Put(const Request* r) {
  assert(r->type == Request::Type::PUT);
  auto idx = r->key_hash % kHashTableSize;
  uint32_t slot = offsetof(NVMTablet, hash_table) + sizeof(uint32_t) * idx;
  auto head = allocator_.Read<uint32_t>(slot);
  auto q = slot, p = head;
  while (p) {
    if (r->key_len != allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, key_len)) ||
        allocator_.Memcmp(OFFSETOF_NVMOBJECT(p, data), r->Key(), r->key_len)) {
      q = p;
      p = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
      continue;
    }
    if (r->val_len < allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, val_len))) {
      // The new value is shorter than the older, store data at its original place.
      allocator_.Write(OFFSETOF_NVMOBJECT(p, val_len), r->val_len);
      allocator_.Memcpy(OFFSETOF_NVMOBJECT(p, data) + r->key_len, r->Val(), r->val_len);
    } else {
      auto next = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
      allocator_.Free(p);
      auto size = sizeof(NVMObject) + r->key_len + r->val_len;
      p = allocator_.Alloc(size);
      // TODO(wgtdkp): handle the situation: `no space`.
      assert(p != 0);
      allocator_.Write<NVMObject>(p, {next, r->key_len, r->val_len, r->key_hash});
      allocator_.Memcpy(OFFSETOF_NVMOBJECT(p, data), r->data, r->key_len + r->val_len);
      allocator_.Write(OFFSETOF_NVMOBJECT(q, next), p);
    }
    break;
  }
  if (!p) {
    auto size = sizeof(NVMObject) + r->key_len + r->val_len;
    p = allocator_.Alloc(size);
    // TODO(wgtdkp): handle the situation: `no space`.
    assert(p != 0);
    // TODO(wgtdkp): use single `memcpy`
    allocator_.Write<NVMObject>(p, {head, r->key_len, r->val_len, r->key_hash});
    allocator_.Memcpy(OFFSETOF_NVMOBJECT(p, data), r->data, r->key_len + r->val_len);
    // Insert the new item to head of the bucket list
    allocator_.Write(slot, p);
  }
  return Status::OK;
}

Status Tablet::Get(Response* resp, const Request* r) {
  assert(r->type == Request::Type::GET);
  auto idx = r->key_hash % kHashTableSize;
  auto p = offsetof(NVMTablet, hash_table) + sizeof(uint32_t) * idx;
  p = allocator_.Read<uint32_t>(p);
  while (p) {
    // TODO(wgtdkp): use a different hash to speedup searching
    if (r->key_len != allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, key_len)) ||
        allocator_.Memcmp(OFFSETOF_NVMOBJECT(p, data), r->Key(), r->key_len)) {
      auto len = allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, val_len));
      allocator_.Memcpy(resp->val, OFFSETOF_NVMOBJECT(p, data) + r->key_len, len);
      resp->val_len = len;
      return Status::OK;
    }
    p = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
  }
  return Status::ERROR;
}

Status Tablet::Del(const Request* r) {
  assert(r->type == Request::Type::DEL);
  auto idx = r->key_hash % kHashTableSize;
  auto q = offsetof(NVMTablet, hash_table) + sizeof(uint32_t) * idx;
  auto p = allocator_.Read<uint32_t>(q);
  while (p) {
    if (r->key_len != allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, key_len)) ||
        allocator_.Memcmp(OFFSETOF_NVMOBJECT(p, data), r->Key(), r->key_len)) {
      auto next = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
      allocator_.Write(OFFSETOF_NVMOBJECT(q, next), next);
      allocator_.Free(p);
      return Status::OK;
    }
  }
  return Status::ERROR;
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
      assert(backup_info.is_backup);
      qps_[i]->Plumb(backup_info.qpis[0]);
    }
  }
}

} // namespace nvds
