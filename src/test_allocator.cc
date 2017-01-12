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

/*
TEST (AllocatorTest, Format) {
  auto mem = malloc(Allocator::kSize);

  uintptr_t base = reinterpret_cast<uintptr_t>(mem);
  Allocator a(base);

  a.Format();

  free(mem);
}

TEST (AllocatorTest, Alloc) {
  auto mem = malloc(Allocator::kSize);

  uintptr_t base = reinterpret_cast<uintptr_t>(mem);
  Allocator a(base);
  a.Format();

  uintptr_t blk;
  vector<uintptr_t> blks;
  while ((blk = a.Alloc(12)) != 0) {
    blks.push_back(blk);
  }
  ASSERT_EQ(4194295, blks.size());
  cout << "average cnt_writes: " << a.cnt_writes() * 1.0 / blks.size() << endl;
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
  ASSERT_EQ(4194295, blks.size());

  while (blks.size() > 0) {
    auto blk = blks.back();
    blks.pop_back();
    a.Free(blk);
  }

  while ((blk = a.Alloc(12)) != 0) {
    blks.push_back(blk);
  }
  ASSERT_EQ(4194295, blks.size());

  free(mem);  
}
*/

TEST (AllocatorTest, Random) {
  auto mem = malloc(Allocator::kSize);

  uintptr_t base = reinterpret_cast<uintptr_t>(mem);
  Allocator a(base);
  a.Format();

  vector<uintptr_t> blks;
  uintptr_t blk;
  auto g = default_random_engine();
  size_t cnt = 0;
  clock_t begin = clock();
  for (size_t i = 0, j = 0, k = 0; i < 1024 * 1024 * 1024; ++i) {
    int op = g() % 2;
    if (op == 0) {
      blk = a.Alloc(g() % (Allocator::kMaxBlockSize - sizeof(uint32_t)));
      if (blk == 0)
        break;
      blks.push_back(blk);
      ++k;
      ++cnt;
    } else if (j < blks.size()) {
      a.Free(blks[j++]);
      ++cnt;
    }
  }
  auto total_time = (clock() - begin) * 1.0 / CLOCKS_PER_SEC;
  cout << "cnt: " << cnt << endl;
  cout << "total time: " << total_time << endl;
  cout << "average time: " << total_time / cnt << endl;
  cout << "average cnt_writes: " << a.cnt_writes() * 1.0 / cnt << endl;

  free(mem);
}

TEST (AllocatorTest, UsageRate) {
  auto mem = malloc(Allocator::kSize);

  uintptr_t base = reinterpret_cast<uintptr_t>(mem);
  Allocator a(base);
  a.Format();

  vector<uintptr_t> blks;
  uintptr_t blk;
  auto g = default_random_engine();
  uint32_t total_size = 0;
  for (size_t i = 0, j = 0; ; ++i) {
    uint32_t item_size = g() % (Allocator::kMaxBlockSize - sizeof(uint32_t));
    blk = a.Alloc(item_size);
    if (blk == 0)
      break;
    total_size += item_size;
    blks.push_back(blk);
  }

  cout << "usage rate: " << total_size * 1.0 / Allocator::kSize << endl;

  free(mem);
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
