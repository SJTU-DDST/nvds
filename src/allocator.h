/*
 * This allocator is a simplified version of `Doug Lea's Malloc`.
 * Reference: http://g.oswego.edu/dl/html/malloc.html
 */

#ifndef _NVDS_ALLOCATOR_H_
#define _NVDS_ALLOCATOR_H_

#include "common.h"

#include <memory.h>

namespace nvds {

class Allocator {
  friend class Tablet;
 public:
  static const uint32_t kMaxBlockSize = 1024 + 128;
  static const uint32_t kSize = 16 * 1024 * 1024;
  Allocator(void* base) : Allocator(reinterpret_cast<uintptr_t>(base)) {
    Format();
  }
  Allocator(uintptr_t base) : base_(base), cnt_writes_(0) {
    flm_ = OffsetToPtr<FreeListManager>(0);
  }
  ~Allocator() {}
  DISALLOW_COPY_AND_ASSIGN(Allocator);

  //uintptr_t Alloc(uint32_t size);
  //void Free(uintptr_t ptr);
  template<typename T>
  NVMPtr<T> Alloc(uint32_t size) {
    auto blk_size = RoundupBlockSize(size + sizeof(uint32_t));
    assert(blk_size % 16 == 0);
    // The client side should refuse too big kv item.  
    assert(blk_size <= kMaxBlockSize);

    auto blk = AllocBlock(blk_size);
    auto ret = blk == 0 ? 0 : blk + sizeof(uint32_t) + base_;
    return NVMPtr<T>(reinterpret_cast<T*>(ret));
  }
  template<typename T>
  void Free(NVMPtr<T> ptr) {
    // TODO(wgtdkp): checking if ptr is actually in this Allocator zone.
    auto p = reinterpret_cast<uintptr_t>(ptr.ptr());
    assert(p >= base_ + sizeof(uint32_t) && p <= base_ + kSize);
    auto blk = p - sizeof(uint32_t) - base_;
    FreeBlock(blk);
  }
  uintptr_t base() const { return base_; }
  uint64_t cnt_writes() const { return cnt_writes_; }

 private:
  static const uint32_t kNumFreeLists = kMaxBlockSize / 16 + 1;
  void Format();
  template<typename T>
  T* OffsetToPtr(uint32_t offset) const {
    return reinterpret_cast<T*>(base_ + offset);
  }
  template<typename T>
  void Write(uint32_t offset, const T& val) {
    assert(offset != 0);
    auto ptr = OffsetToPtr<T>(offset);
    *ptr = val;
    // TODO(wgtdkp): Collect
    ++cnt_writes_;
  }
  template<typename T>
  T Read(uint32_t offset) {
    assert(offset);
    return *OffsetToPtr<T>(offset);
  }
  // TODO(wgtdkp): add latency
  int Memcmp(uint32_t lhs, uint32_t rhs, uint32_t len) {
    return memcmp(OffsetToPtr<char>(lhs), OffsetToPtr<char>(rhs), len);
  }
  int Memcmp(uint32_t lhs, const char* rhs, uint32_t len) {
    return memcmp(OffsetToPtr<char>(lhs), rhs, len);
  }
  int Memcmp(const char* lhs, uint32_t rhs, uint32_t len) {
    return memcmp(lhs, OffsetToPtr<char>(rhs), len);
  }
  void Memcpy(uint32_t des, uint32_t src, uint32_t len) {
    memcpy(OffsetToPtr<char>(des), OffsetToPtr<char>(src), len);
  }
  void Memcpy(uint32_t des, const char* src, uint32_t len) {
    memcpy(OffsetToPtr<char>(des), src, len);
  }
  void Memcpy(char* des, uint32_t src, uint32_t len) {
    memcpy(des, OffsetToPtr<char>(src), len);
  }

  PACKED(
  struct BlockHeader {
    // Denoting if previous block is free.
    uint32_t free: 1;
    // Block size, include block header.
    uint32_t size: 31;
    // Pointer to previous block in the same free list.
    // Relative to base_.
    uint32_t prev;
    // Pointer to next block in the same free list.
    // Relative to base_.
    uint32_t next;

    static const uint32_t kFreeMask = static_cast<uint32_t>(1) << 31;
    BlockHeader() = delete;
  });

  PACKED(
  struct BlockFooter {
    // Denoting if current block is free.
    uint32_t free: 1;
    // Block size, include block header.
    uint32_t size: 31;

    BlockFooter() = delete;
  });

  PACKED(
  struct FreeListManager {
    // The list of these free blocks with same size.
    // The last list is all free block greater than kMaxBlockSize.
    // So these blocks are not necessary the same size,
    // and not sorted(by addr or size).
    uint32_t free_lists[kNumFreeLists];

    FreeListManager() = delete;
  });

  uint32_t GetBlockSizeByFreeList(uint32_t free_list) {
    free_list /= sizeof(uint32_t);
    if (free_list < kNumFreeLists - 1) {
      return (free_list + 1) * 16;
    }
    assert(free_list == kNumFreeLists - 1);
    auto head = Read<uint32_t>(free_list * sizeof(uint32_t));
    return head == 0 ? 0 : ReadTheSizeTag(head);
  }
  uint32_t GetFreeListByBlockOffset(uint32_t blk) {
    auto blk_size = OffsetToPtr<const BlockHeader>(blk)->size;
    return GetFreeListByBlockSize(blk_size);
  }
  uint32_t GetFreeListByBlockSize(uint32_t blk_size) {
    assert(blk_size >= 16);
    if (blk_size <= kMaxBlockSize) {
      return (blk_size / 16 - 1) * sizeof(uint32_t);
    }
    return GetLastFreeList();
  }
  static uint32_t RoundupBlockSize(uint32_t blk_size) {
    if (blk_size <= kMaxBlockSize) {
      return (blk_size + 16 - 1) / 16 * 16;
    }
    assert(false);
    return 0;
  }
  static uint32_t GetLastFreeList() {
    return (kNumFreeLists - 1) * sizeof(uint32_t);
  }
  void SetTheFreeTag(uint32_t blk, uint32_t blk_size) {
    auto next_blk = blk + blk_size;
    Write(next_blk, Read<uint32_t>(next_blk) | BlockHeader::kFreeMask);
  }
  void ResetTheFreeTag(uint32_t blk, uint32_t blk_size) {
    auto next_blk = blk + blk_size;
    Write(next_blk, Read<uint32_t>(next_blk) & ~BlockHeader::kFreeMask);
  }
  uint32_t ReadTheFreeTag(uint32_t blk, uint32_t blk_size) {
    return Read<uint32_t>(blk + blk_size) & BlockHeader::kFreeMask;
  }
  void SetThePrevFreeTag(uint32_t blk) {
    Write(blk, BlockHeader::kFreeMask | Read<uint32_t>(blk));
  }
  void ResetThePrevFreeTag(uint32_t blk) {
    Write(blk, ~BlockHeader::kFreeMask & Read<uint32_t>(blk));
  }
  uint32_t ReadThePrevFreeTag(uint32_t blk) {
    return Read<uint32_t>(blk) & BlockHeader::kFreeMask;
  }
  uint32_t ReadTheSizeTag(uint32_t blk) {
    return Read<uint32_t>(blk) & ~BlockHeader::kFreeMask;
  }

  uint32_t AllocBlock(uint32_t size);
  void FreeBlock(uint32_t blk);
  
  // Remove the block from the free list, and it keeps free.
  void RemoveBlock(uint32_t blk, uint32_t blk_size);
  uint32_t SplitBlock(uint32_t blk, uint32_t blk_size, uint32_t needed_size);

 private:
  uintptr_t base_;
  FreeListManager* flm_;
  uint64_t cnt_writes_;
};

} // namespace nvds

#endif // _NVDS_ALLOCATOR_H_
