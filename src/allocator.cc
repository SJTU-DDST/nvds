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

uint32_t Allocator::AllocBlock(uint32_t block_size) {
  auto& free_lists = flm_->free_lists;
  uint32_t ret = 0;
  uint32_t idx = block_size / 16 - 1;
  for (uint32_t i = idx; i < kNumFreeList; ++i) {
    if (free_lists[idx] != 0) {
      auto blk = OffsetToPtr<BlockHeader>(free_lists[idx]);
      ret = free_lists[idx] + sizeof(BlockHeader);
      // TODO(wgtdkp): Collect writing
      free_lists[idx] = blk->next;
      blk = OffsetToPtr<BlockHeader>(free_lists[idx]);
      // TODO(wgtdkp): Collect writing
      blk->prev = 0;
      break;
    }
  }
  return ret;
}

void Allocator::FreeBlock(uint32_t block_ptr) {

}

} // namespace nvds
