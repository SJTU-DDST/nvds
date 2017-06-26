// clang++ -std=c++11 -Wall -o memc -g memc.cc -lmemcached -lrt

#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <libmemcached/memcached.h>

using namespace std;

static vector<string> GenRandomStrings(size_t len, size_t n) {
  vector<string> ans;
  default_random_engine g;
  uniform_int_distribution<int> dist('a', 'z');
  for (size_t i = 0; i < n; ++i) {
    ans.emplace_back(len, 0);
    for (size_t j = 0; j < len; ++j) {
      ans[i][j] = dist(g);
    }
  }
  return ans;
}

static inline int64_t TimeDiff(struct timespec* lhs, struct timespec* rhs) {
  return (lhs->tv_sec - rhs->tv_sec) * 1000000000 + 
         (lhs->tv_nsec - rhs->tv_nsec);
}

void Test(const char* server, uint32_t len) {
  auto cfg = "--SERVER=" + string(server);
  memcached_st* memc = memcached(cfg.c_str(), cfg.size());
  assert(memc != nullptr);

  static const size_t n = 10000;

  auto keys = GenRandomStrings(16, n);
  auto vals = GenRandomStrings(len, n);
  
  struct timespec begin, end;
  assert(clock_gettime(CLOCK_REALTIME, &begin) == 0);
  for (size_t i = 0; i < keys.size(); ++i) {
    auto& key = keys[i];
    auto& val = vals[i];
    memcached_return_t rc = memcached_set(memc, key.c_str(), key.size(), val.c_str(), val.size(), 0, 0);
    assert(rc == MEMCACHED_SUCCESS);
  }
  assert(clock_gettime(CLOCK_REALTIME, &end) == 0);
  double t_set = TimeDiff(&end, &begin) * 1.0 / n / 1000;

  assert(clock_gettime(CLOCK_REALTIME, &begin) == 0);
  for (size_t i = 0; i < keys.size(); ++i) {
    auto& key = keys[i];
    size_t val_len;
    uint32_t flags;
    memcached_return_t rc;
    memcached_get(memc, key.c_str(), key.size(), &val_len, &flags, &rc);
    assert(rc == MEMCACHED_SUCCESS || rc == MEMCACHED_NOTFOUND);
  }
  assert(clock_gettime(CLOCK_REALTIME, &end) == 0);
  double t_get = TimeDiff(&end, &begin) * 1.0 / n / 1000;

  memcached_free(memc);

  printf("%u, %f, %f\n", len, t_set, t_get);
}

int main(int argc, const char* argv[]) {
  assert(argc == 3);

  const char* server = argv[1];
  uint32_t len = atoi(argv[2]);

  //for (uint32_t len = 16; len <= 1024; len <<= 1) {
    Test(server, len);
  //}
  return 0;
}
