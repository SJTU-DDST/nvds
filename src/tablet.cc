#include "tablet.h"

#include "index.h"
#include "request.h"

#include <algorithm>

#define OFFSETOF_NVMOBJECT(obj, member) \
    offsetof(NVMObject, member) + obj

namespace nvds {

using Status = Response::Status;

Tablet::Tablet(const IndexManager& index_manager,
               NVMPtr<NVMTablet> nvm_tablet, bool is_backup)
    : index_manager_(index_manager),
      nvm_tablet_(nvm_tablet), allocator_(&nvm_tablet->data) {
  info_.is_backup = is_backup;
  
  // Memory region
  // FIXME(wgtdkp): how to simulate latency of RDMA read/write to NVM?
  mr_ = ibv_reg_mr(ib_.pd(), nvm_tablet_.ptr(), kNVMTabletSize,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  assert(mr_ != nullptr);

  for (size_t i = 0; i < kNumReplicas; ++i) {
    qps_[i] = new Infiniband::QueuePair(ib_, IBV_QPT_RC,
        kMaxIBQueueDepth, kMaxIBQueueDepth);
    //qps_[i]->Plumb();
    info_.qpis[i] = {
      ib_.GetLid(Infiniband::kPort),
      qps_[i]->GetLocalQPNum(),
      Infiniband::QueuePair::kDefaultPsn,
      mr_->rkey,
      reinterpret_cast<uint64_t>(nvm_tablet_.ptr())
    };
  }

  log_offsets_.fill(0);
}

Tablet::~Tablet() {
  for (ssize_t i = kNumReplicas - 1; i >= 0; --i) {
    delete qps_[i];
  }
  int err = ibv_dereg_mr(mr_);
  assert(err == 0);
}

Status Tablet::Put(const Request* r, ModificationList& modifications) {
  allocator_.set_modifications(&modifications);

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

Status Tablet::Get(Response* resp, const Request* r,
                   ModificationList& modifications) {
  allocator_.set_modifications(&modifications);

  assert(r->type == Request::Type::GET);
  auto idx = r->key_hash % kHashTableSize;
  auto p = offsetof(NVMTablet, hash_table) + sizeof(uint32_t) * idx;
  p = allocator_.Read<uint32_t>(p);
  while (p) {
    // TODO(wgtdkp): use a different hash to speedup searching
    if (r->key_len == allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, key_len)) &&
        allocator_.Memcmp(OFFSETOF_NVMOBJECT(p, data), r->Key(), r->key_len) == 0) {
      auto len = allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, val_len));
      allocator_.Memcpy(resp->val, OFFSETOF_NVMOBJECT(p, data) + r->key_len, len);
      resp->val_len = len;
      return Status::OK;
    }
    p = allocator_.Read<uint32_t>(OFFSETOF_NVMOBJECT(p, next));
  }
  return Status::ERROR;
}

Status Tablet::Del(const Request* r, ModificationList& modifications) {
  allocator_.set_modifications(&modifications);

  assert(r->type == Request::Type::DEL);
  auto idx = r->key_hash % kHashTableSize;
  auto q = offsetof(NVMTablet, hash_table) + sizeof(uint32_t) * idx;
  auto p = allocator_.Read<uint32_t>(q);
  while (p) {
    if (r->key_len == allocator_.Read<uint16_t>(OFFSETOF_NVMOBJECT(p, key_len)) &&
        allocator_.Memcmp(OFFSETOF_NVMOBJECT(p, data), r->Key(), r->key_len) == 0) {
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
        qps_[0]->SetStateRTR(master_info.qpis[i]);
        break;
      }
      assert(i < kNumReplicas - 1);
    }
  } else {
    for (uint32_t i = 0; i < kNumReplicas; ++i) {
      auto backup_id = info_.backups[i];
      auto backup_info = index_manager.GetTablet(backup_id);
      assert(backup_info.is_backup);
      qps_[i]->SetStateRTS(backup_info.qpis[0]);
    }
  }
}

int Tablet::Sync(Infiniband::Buffer* b, ModificationList& modifications) {
  if (modifications.size() == 0) {
    return 0;
  }
  MergeModifications(modifications);

  for (size_t k = 0; k < info_.backups.size(); ++k) {
    auto backup = index_manager_.GetTablet(info_.backups[k]);
    assert(backup.is_backup);
    
    auto log = reinterpret_cast<ModificationLog*>(b->buf);
    MakeLog(log, modifications);
  
    auto num_sge = MakeSGEs(&sges_[0], b->mr, log, modifications);
    auto len_log = std::accumulate(sges_.begin(), sges_.begin() + num_sge, 0,
        [](size_t acc, struct ibv_sge& x) { return acc + x.length; });
    // DEBUG
    //std::cout << "len_log: " << len_log << std::endl;

    auto& wr = wrs_[0];
    wr.wr.rdma.remote_addr = backup.qpis[0].vaddr + offsetof(NVMTablet, log) + log_offsets_[k];
    wr.wr.rdma.rkey        = backup.qpis[0].rkey;
    wr.wr_id               = 1;
    wr.sg_list             = &sges_[0];
    wr.num_sge             = num_sge;
    wr.opcode              = IBV_WR_RDMA_WRITE;
    wr.send_flags          = IBV_SEND_INLINE | IBV_SEND_SIGNALED;
    wr.next                = nullptr;

    struct ibv_send_wr* bad_wr;
    int err = ibv_post_send(qps_[k]->qp, &wr, &bad_wr);
    if (err != 0) {
      throw TransportException(HERE, "ibv_post_send failed", err);
    }

    log_offsets_[k] += len_log;
    if (log_offsets_[k] > kLogSize) {
      log_offsets_[k] = 0;
    }
  }

  for (auto& qp : qps_) {
    ibv_wc wc;
    int r;
    while ((r = ibv_poll_cq(qp->scq, 1, &wc)) != 1) {}
    if (wc.status != IBV_WC_SUCCESS) {
      throw TransportException(HERE, wc.status);
    } else {
      // std::clog << "success" << std::endl << std::flush;
    }
  }
  return 0;
}

// DEBUG
static void PrintModifications(const ModificationList& modifications) {
  for (const auto& m : modifications) {
    m.Print();
  }
  std::cout << std::endl << std::flush;
}

void Tablet::MergeModifications(ModificationList& modifications) {
  auto n = modifications.size();
  assert(n > 0);

  //PrintModifications(modifications);

  // Baby, don't worry, n <= 12.
  // This simple algorithm should be faster than `union-find`;
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = i + 1; j < n; ++j) {
      if (modifications[j] < modifications[i])
        std::swap(modifications[j], modifications[i]);
    }
  }

  //PrintModifications(modifications);

  static const size_t gap = 16;
  size_t i = 0;
  uint64_t src = modifications[0].src;
  uint32_t begin = modifications[0].des; 
  uint32_t end = modifications[0].des + modifications[0].len;
  for (const auto& m : modifications) {
    if (m.des > end + gap) {
      modifications[i++] = {begin, src, end - begin};
      begin = m.des;
      end = m.des + m.len;
      src = m.src;
    } else {
      end = std::max(end, m.des + m.len);
    }
  }
  modifications[i++] = {begin, src, end - begin};
  modifications.resize(i);

  // PrintModifications(modifications);
}

void Tablet::MakeLog(ModificationLog* log, const ModificationList& modifications) {
  log->cnt = modifications.size();
  size_t i = 0;
  for (const auto& m : modifications) {
    log->positions[i] = {m.des, m.len};
  }
}

size_t Tablet::MakeSGEs(struct ibv_sge* sges, struct ibv_mr* mr,
                        const ModificationLog* log,
                        const ModificationList& modifications) {
  size_t i = 0;
  sges[i++] = {
    reinterpret_cast<uint64_t>(log),
    sizeof(ModificationLog) + log->cnt * sizeof(Position),
    mr->lkey
  };
  for (const auto& m : modifications) {
    sges[i++] = {
      m.src, m.len, mr_->lkey
    };
  }
  return i;
}

} // namespace nvds
