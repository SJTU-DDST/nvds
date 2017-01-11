#include "allocator.h"

#include <cstring>

namespace nvds {

uintptr_t Allocator::Alloc(uint32_t size) {
  // The client side should refuse too big kv item.
  // TODO(wgtdkp): round up.
  auto blk_size = size + sizeof(uint32_t);
  assert(blk_size % 16 == 0);
  assert(blk_size <= kMaxBlockSize);

  auto blk = AllocBlock(blk_size);
  return blk == 0 ? 0 : blk + sizeof(uint32_t) + base_;
}

// A nullptr(0) is not going to be accepted.
void Allocator::Free(uintptr_t ptr) {
  // TODO(wgtdkp): Checking if ptr is actually in this Allocator zone.
  assert(ptr >= base_ + sizeof(uint32_t) && ptr <= base_ + kSize);
  auto blk = ptr - sizeof(uint32_t) - base_;
  FreeBlock(blk);
}

void Allocator::Format() {
  static_assert(sizeof(FreeListManager) == 512 * 1024,
                "Wrong FreeListManager size");
  memset(flm_, 0, sizeof(FreeListManager));
  uint32_t free_list = (kNumFreeList / 2 - 1) * sizeof(uint32_t);
  uint32_t blk = sizeof(FreeListManager);

  // 'The block before first block' is not free
  Write(blk, ~BlockHeader::kFreeMask & kMaxBlockSize);
  for (; blk + kMaxBlockSize + sizeof(uint32_t) <= kSize; blk += kMaxBlockSize) {
    auto head = Read<uint32_t>(free_list);
    if (head != 0) {
      Write(head + offsetof(BlockHeader, prev), blk);
    }

    Write(blk + offsetof(BlockHeader, prev), static_cast<uint32_t>(0));    
    Write(blk + offsetof(BlockHeader, next), head);

    Write(blk, ReadThePrevFreeTag(blk) | kMaxBlockSize);
    Write(blk + kMaxBlockSize - sizeof(uint32_t), kMaxBlockSize);
    SetTheFreeTag(blk, kMaxBlockSize);
    Write(free_list, blk);
  }
}

uint32_t Allocator::AllocBlock(uint32_t blk_size) {
  uint32_t idx = blk_size / 16 - 1;
  for (uint32_t i = idx; i < kNumFreeList; ++i) {
    auto free_list = i * sizeof(uint32_t);
    auto head = Read<uint32_t>(free_list);
    auto head_size = (i + 1) * 16;
    if (head == 0)
      continue;
    if (i == idx) {
      auto next_blk = Read<uint32_t>(head + offsetof(BlockHeader, next));
      ResetTheFreeTag(head, head_size);
      Write(free_list, next_blk);
      Write(next_blk + offsetof(BlockHeader, prev),
            static_cast<uint32_t>(0));
      return head;
    } else {
      if (free_list == 262140) {
        free_list = 262140;
      }
      return SplitBlock(head, head_size, blk_size);
    }
  }
  return 0;
}

void Allocator::FreeBlock(uint32_t blk) {
  auto blk_size      = Read<uint32_t>(blk) & ~BlockHeader::kFreeMask;
  auto blk_size_old  = blk_size;
  auto prev_blk_size = Read<uint32_t>(blk - sizeof(uint32_t));
  auto prev_blk      = blk - prev_blk_size;
  auto next_blk      = blk + blk_size;
  auto next_blk_size = Read<uint32_t>(next_blk) & ~BlockHeader::kFreeMask;

  if (ReadThePrevFreeTag(blk) &&
      blk_size + prev_blk_size <= 2 * kMaxBlockSize) {
    RemoveBlock(prev_blk, prev_blk_size);
    blk = prev_blk;
    blk_size += prev_blk_size;
  }
  if (ReadTheFreeTag(next_blk, next_blk_size) &&
      blk_size + next_blk_size <= 2 * kMaxBlockSize) {
    RemoveBlock(next_blk, next_blk_size);
    blk_size += next_blk_size;
  }

  // If the blk_size changed, write it.
  if (blk_size != blk_size_old) {
    Write(blk, ReadThePrevFreeTag(blk) | blk_size);
    Write(blk + blk_size - sizeof(uint32_t), blk_size);
  }
  SetTheFreeTag(blk, blk_size);

  auto free_list = GetFreeListByBlockSize(blk_size);
  auto head = Read<uint32_t>(free_list);
  Write(free_list, blk);
  Write(blk + offsetof(BlockHeader, prev), static_cast<uint32_t>(0));
  Write(blk + offsetof(BlockHeader, next), head);
  if (head != 0) {
    Write(head + offsetof(BlockHeader, prev), blk);
  }
}

uint32_t Allocator::SplitBlock(uint32_t blk,
                               uint32_t blk_size,
                               uint32_t needed_size) {
  auto next_blk  = Read<uint32_t>(blk + offsetof(BlockHeader, next));
  auto free_list = GetFreeListByBlockSize(blk_size);
  auto new_size  = blk_size - needed_size;
  
  Write(free_list, next_blk);
  if (next_blk != 0) {
    Write(next_blk + offsetof(BlockHeader, prev), static_cast<uint32_t>(0));
  }

  // Update size
  Write(blk, ReadThePrevFreeTag(blk) | new_size);
  Write(blk + new_size - sizeof(uint32_t), new_size);
  // Free the block after updating size
  //FreeBlock(blk);

  free_list = GetFreeListByBlockSize(new_size);
  auto head = Read<uint32_t>(free_list);
  Write(blk + offsetof(BlockHeader, prev), static_cast<uint32_t>(0));
  Write(blk + offsetof(BlockHeader, next), head);
  Write(free_list, blk);

  // The rest smaller block is still free  
  Write(blk + new_size, BlockHeader::kFreeMask | needed_size);
  // Piece splitted is not free now.
  ResetTheFreeTag(blk, blk_size);
  return blk + new_size;
}

} // namespace nvds
