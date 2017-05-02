#include "allocator.h"

#include <cstring>

namespace nvds {

void Allocator::Format() {
  memset(flm_, 0, sizeof(FreeListManager));
  uint32_t blk = sizeof(FreeListManager);
  Write(GetLastFreeList(), blk);

  // 'The block before first block' is not free
  // The last 4 bytes is kept for the free tag
  uint32_t blk_size = (kSize - blk - sizeof(uint32_t)) &
                      ~static_cast<uint32_t>(0x0f);
  Write(blk, blk_size & ~BlockHeader::kFreeMask);
  Write(blk + blk_size - sizeof(uint32_t), blk_size);
  SetTheFreeTag(blk, blk_size);
}

uint32_t Allocator::AllocBlock(uint32_t blk_size) {
  auto free_list = GetFreeListByBlockSize(blk_size);
  uint32_t head;
  while (free_list < kNumFreeLists * sizeof(uint32_t) &&
         (head = Read<uint32_t>(free_list)) == 0) {
    free_list += sizeof(uint32_t);
  }
  if (head == 0) {
    return 0;
  }
  auto head_size = ReadTheSizeTag(head);
  assert(ReadTheFreeTag(head, head_size) != 0);
  assert(head_size >= blk_size);
  if (head_size == blk_size) {
    auto next_blk = Read<uint32_t>(head + offsetof(BlockHeader, next));
    ResetTheFreeTag(head, head_size);
    Write(free_list, next_blk);
    Write(next_blk + offsetof(BlockHeader, prev),
          static_cast<uint32_t>(0));
    return head;
  }
  return SplitBlock(head, head_size, blk_size);
}

void Allocator::FreeBlock(uint32_t blk) {
  auto blk_size      = ReadTheSizeTag(blk);
  auto blk_size_old  = blk_size;
  auto prev_blk_size = Read<uint32_t>(blk - sizeof(uint32_t));
  auto prev_blk      = blk - prev_blk_size;
  auto next_blk      = blk + blk_size;
  auto next_blk_size = ReadTheSizeTag(next_blk);

  if (ReadThePrevFreeTag(blk) &&
      blk_size <= kMaxBlockSize &&
      prev_blk_size <= kMaxBlockSize) {
    RemoveBlock(prev_blk, prev_blk_size);
    blk = prev_blk;
    blk_size += prev_blk_size;
  }
  if (ReadTheFreeTag(next_blk, next_blk_size) &&
      blk_size <= kMaxBlockSize &&
      next_blk_size <= kMaxBlockSize) {
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

void Allocator::RemoveBlock(uint32_t blk, uint32_t blk_size) {
  assert(ReadTheFreeTag(blk, blk_size) != 0);
  auto prev_blk = Read<uint32_t>(blk + offsetof(BlockHeader, prev));
  auto next_blk = Read<uint32_t>(blk + offsetof(BlockHeader, next));
  if (prev_blk != 0) {
    Write(prev_blk + offsetof(BlockHeader, next),
          Read<uint32_t>(blk + offsetof(BlockHeader, next)));
  } else {
    auto free_list = GetFreeListByBlockSize(blk_size);
    Write(free_list, Read<uint32_t>(blk + offsetof(BlockHeader, next)));
  }
  if (next_blk != 0) {
    Write(next_blk + offsetof(BlockHeader, prev),
          Read<uint32_t>(blk + offsetof(BlockHeader, prev)));
  }
}

uint32_t Allocator::SplitBlock(uint32_t blk,
                               uint32_t blk_size,
                               uint32_t needed_size) {
  auto next_blk  = Read<uint32_t>(blk + offsetof(BlockHeader, next));
  auto free_list = GetFreeListByBlockSize(blk_size);
  assert(blk_size > needed_size);
  auto new_size  = blk_size - needed_size;

  Write(free_list, next_blk);
  if (next_blk != 0) {
    Write(next_blk + offsetof(BlockHeader, prev), static_cast<uint32_t>(0));
  }

  // Update size
  Write(blk, ReadThePrevFreeTag(blk) | new_size);
  Write(blk + new_size - sizeof(uint32_t), new_size);

  free_list = GetFreeListByBlockSize(new_size);
  auto head = Read<uint32_t>(free_list);
  Write(blk + offsetof(BlockHeader, prev), static_cast<uint32_t>(0));
  Write(blk + offsetof(BlockHeader, next), head);
  Write(free_list, blk);
  if (head != 0) {
    Write(head + offsetof(BlockHeader, prev), blk);
  }

  // The rest smaller block is still free
  Write(blk + new_size, BlockHeader::kFreeMask | needed_size);
  Write(blk + blk_size - sizeof(uint32_t), needed_size);

  // Piece splitted is not free now.
  ResetTheFreeTag(blk + new_size, needed_size);
  return blk + new_size;
}

} // namespace nvds
