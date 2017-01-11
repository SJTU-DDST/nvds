#include "allocator.h"
#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace std;
using namespace nvds;

TEST (AllocatorTest, Init) {
  auto mem = malloc(Allocator::kSize);

  uintptr_t base = reinterpret_cast<uintptr_t>(mem);
  Allocator a(base);
  free(mem);
}

TEST (AllocatorTest, Format) {
  auto mem = malloc(Allocator::kSize);

  uintptr_t base = reinterpret_cast<uintptr_t>(mem);
  Allocator a(base);

  a.Format();

  free(mem);
}

TEST (AllocatorTest, Free) {
  auto mem = malloc(Allocator::kSize);

  uintptr_t base = reinterpret_cast<uintptr_t>(mem);
  Allocator a(base);
  a.Format();

  uintptr_t blk;
  for (size_t i = 0; i < 2 * 128 * 1024; i += 2) {
    blk = a.Alloc(12);
    a.Free(blk);
  }

  vector<uintptr_t> blks;
  while ((blk = a.Alloc(12)) != 0) {
    blks.push_back(blk);
  }
  ASSERT_EQ((Allocator::kSize - 1024 * 1024) / 16, blks.size());

  free(mem);
}

TEST (AllocatorTest, AllocFree) {
  auto mem = malloc(Allocator::kSize);

  uintptr_t base = reinterpret_cast<uintptr_t>(mem);
  Allocator a(base);
  a.Format();

  vector<uintptr_t> blks;
  uintptr_t blk;
  while ((blk = a.Alloc(12)) != 0) {
    blks.push_back(blk);
  }
  ASSERT_EQ((Allocator::kSize - 1024 * 1024) / 16, blks.size());

  while (blks.size() > 0) {
    auto blk = blks.back();
    blks.pop_back();
    a.Free(blk);
  }

  while ((blk = a.Alloc(12)) != 0) {
    blks.push_back(blk);
  }
  ASSERT_EQ((Allocator::kSize - 1024 * 1024) / 16, blks.size());

  free(mem);  
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
