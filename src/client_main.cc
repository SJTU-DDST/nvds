#include "client.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

using namespace nvds;

using VecStr = std::vector<std::string>;

static void Usage() {
  std::cout << "usage: " << std::endl;
  std::cout << "client  <coordinator_addr> <item number per thread> <thread numer>" << std::endl;
}

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

void Work(std::shared_ptr<Client> c, const VecStr& keys, const VecStr& vals) {
  try {
    auto begin = clock();
    for (size_t i = 0; i < keys.size(); ++i) {
      c->Put(keys[i], vals[i]);
    }
    double t = 1.0 * (clock() - begin) / CLOCKS_PER_SEC;
    double qps = keys.size() / t;
    // std::cout << "time: " << t << std::endl;
    std::cout << "QPS: " << std::fixed << std::setprecision(1) << qps << std::endl;
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("initialize infiniband devices failed");
  }
}

int main(int argc, const char* argv[]) {
  if (argc < 4) {
    Usage();
    return -1;
  }

  const std::string coord_addr = argv[1];
  const size_t num_items = std::stoi(argv[2]);
  const size_t num_threads = std::stoi(argv[3]);
  
  std::vector<VecStr> keys, vals;
  for (size_t i = 0; i < num_threads; ++i) {
    keys.emplace_back(GenRandomStrings(8, num_items));
    vals.emplace_back(GenRandomStrings(64, num_items));
  }
  std::vector<std::thread> workers;
  std::vector<std::shared_ptr<Client>> clients;
  Work(std::make_shared<Client>(coord_addr), keys[0], vals[0]);
  /*
  for (size_t i = 0; i < num_threads; ++i) {
    clients.emplace_back(std::make_shared<Client>(coord_addr));
  }
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back(std::bind(Work, clients[i], keys[i], vals[i]));
  }
  for (size_t i = 0; i < num_threads; ++i) {
    workers[i].join();
  }
  */
  return 0;
}
