#include "hash.h"
#include "measurement.h"

#include <gtest/gtest.h>
#include <bits/stdc++.h>

using namespace std;
using namespace nvds;

static vector<string> GenRandomStrings(size_t len, size_t n) {
  vector<string> ans;
  std::default_random_engine g;
  std::uniform_int_distribution<int> dist('a', 'z');
  for (size_t i = 0; i < n; ++i) {
    ans.emplace_back(len, 0);
    for (size_t j = 0; j < len; ++j) {
      ans[i][j] = dist(g);
    }
  }
  return ans;
}

TEST (AllocatorTest, Alloc) {
  const size_t n = 1000 * 1000;
  auto vals = GenRandomStrings(40, n);
  vector<uint64_t> keys(n, 0);
  transform(vals.begin(), vals.end(), keys.begin(), [](const string& x) -> uint64_t {
      return Hash(x.substr(0, 16));
    });
  
  Measurement m;
  unordered_map<uint64_t, string> um;
  um.reserve(n);
  m.begin();
  for (size_t i = 0; i < n; ++i) {
    um[keys[i]] = vals[i];
  }
  m.end();
  m.Print();
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
