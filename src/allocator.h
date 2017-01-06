#ifndef _NVDS_ALLOCATOR_H_
#define _NVDS_ALLOCATOR_H_

#include "common.h"

namespace nvds {

class Allocator {
 public:
  static const uint32_t kSize = 64 * 1024 * 1024;
  ~Allocator() {}
  DISALLOW_COPY_AND_ASSIGN(Allocator);

  uintptr_t Alloc(uint32_t size);
  void Free(uintptr_t ptr);

  uintptr_t base() const { return base_; }
  uint32_t size() const { return size_; }

 private:
  static const uint32_t kNumFreeList = 2 * kMaxItemSize / 16;
  Allocator(uintptr_t base, uint32_t size)
    : base_(base), size_(size) {
    flm_ = OffsetToPtr<FreeListManager>(0);
  }

  template<typename T>
  T* OffsetToPtr(uint32_t offset) const {
    return reinterpret_cast<T*>(base_ + offset);
  }

  template<typename T>
  void Write(uint32_t offset, T val) {
    auto ptr = OffsetToPtr<T>(offset);
    *ptr = val;
    // TODO(wgtdkp): Collect
  }

  template<typename T>
  T Read(uint32_t offset) {
    return *OffsetToPtr<T>(offset);
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

    BlockHeader() {}
    ~BlockHeader() {}
  });

  PACKED(
  struct BlockFooter {
    // Denoting if current block is free.
    uint32_t free: 1;
    // Block size, include block header.
    uint32_t size: 31;

    BlockFooter() {}
    ~BlockFooter() {}
  });

  PACKED(
  struct FreeListManager {
    uint32_t free_lists[kNumFreeList];
  });

  uint32_t AllocBlock(uint32_t size);
  void FreeBlock(uint32_t ptr);
  uint32_t SplitBlock(uint32_t blk_offset, uint32_t needed_size);

 private:
  uintptr_t base_;
  uint32_t size_;
  FreeListManager* flm_;
};

} // namespace nvds

#endif // _NVDS_ALLOCATOR_H_
