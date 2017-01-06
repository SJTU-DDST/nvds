#include "allocator.h"

namespace nvds {

uintptr_t Allocator::Alloc(uint32_t size) {
  // The client side should refuse too big kv item.
  assert(size <= kMaxItemSize);

  return 0;
}

void Allocator::Free(uintptr_t ptr) {
  // TODO(wgtdkp): Checking if ptr is actually in this Allocator zone.

}

uint32_t Allocator::AllocBlock(uint32_t blk_size) {
  auto& free_lists = flm_->free_lists;
  uint32_t idx = blk_size / 16 - 1;
  for (uint32_t i = idx; i < kNumFreeList; ++i) {
    if (free_lists[idx] != 0) {
      if (i == idx) {
        auto next_blk =
            Read<uint32_t>(free_lists[idx] + offsetof(BlockHeader, next));
        auto ret = free_lists[idx];
        // FIXME(wgtdkp): assume little endian.
        Write(free_lists[idx] + blk_size,
              (~(static_cast<uint32_t>(1) << 31)) &
                Read<uint32_t>(free_lists[idx] + blk_size));
        // FIXEME(wgtdkp): totally wrong!!!
        //Write(free_lists[idx], next_blk);
        Write(next_blk + offsetof(BlockHeader, prev),
              static_cast<uint32_t>(0));
        return ret;
      } else {
        return SplitBlock(free_lists[idx], blk_size);
      }
    }
  }
  return 0;
}

void Allocator::FreeBlock(uint32_t blk_ptr) {

}

uint32_t Allocator::SplitBlock(uint32_t blk_offset, uint32_t needed_size) {
  auto blk = OffsetToPtr<const BlockHeader>(blk_offset);
  auto new_size = blk->size - needed_size;
  Write(blk_offset, (static_cast<uint32_t>(blk->free) << 31) |
                    (~(static_cast<uint32_t>(1) << 31) & new_size));
  Write(blk_offset + new_size - sizeof(uint32_t), new_size);
  Write(blk_offset + new_size, static_cast<uint32_t>(1) << 31 | needed_size);
}

} // namespace nvds
