#include <libmemcached/memcached.h>

#include <cassert>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <random>

using VecStr = std::vector<std::string>;

static VecStr GenRandomStrings(size_t len, size_t n) {
  VecStr ans;
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

void test(size_t n) {
  const char* config_string = "--SERVER=127.0.0.1";
  memcached_st* memc = memcached(config_string, strlen(config_string));

  auto keys = GenRandomStrings(16, n);
  auto vals = VecStr(n, std::string(16, 'a'));
  auto begin = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < n; ++i) {
    auto& key = keys[i];
    auto& val = vals[i];
    memcached_return_t rc = memcached_set(memc, key.c_str(), key.size(), val.c_str(), val.size(), 0, 0);
    assert(rc == MEMCACHED_SUCCESS);
  }
  auto end = std::chrono::high_resolution_clock::now();
  double t = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin).count();
  double qps = keys.size() / t;

  std::cout << "time: " << t << std::endl;
  std::cout << "qps:  " << qps << std::endl;

  memcached_free(memc);
}

int main() {
  test(10000);
  return 0;
}
