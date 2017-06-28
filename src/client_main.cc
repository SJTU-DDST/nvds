#include "client.h"

#include "measurement.h"

#include <iostream>
#include <memory>
#include <random>
#include <thread>

using namespace nvds;

using VecStr = std::vector<std::string>;

static Client* client;

static void SigInt(int signo) {
  std::cout << std::endl << "num_send: " << client->num_send() << std::endl;
  exit(0);
}

static void Usage() {
  std::cout << "usage: " << std::endl;
  std::cout << "client  <coordinator_addr> <item number per thread> <val_len> <thread numer>" << std::endl;
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

static void Work(Measurement* m, const std::string& coord_addr, size_t n, size_t len_val) {
  using namespace std::chrono;
  try {
    Client c {coord_addr};
    client = &c;
    auto keys = GenRandomStrings(16, n);
    auto vals = VecStr(n, std::string(len_val, 'a'));
    for (size_t i = 0; i < keys.size(); ++i) {
      m->begin();
      c.Put(keys[i], vals[i]);
      //std::cout << "PUT" << std::endl;
      m->end();
    }
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("initialize infiniband devices failed");
  }
}

static int bench_main(int argc, const char* argv[]) {
  if (argc < 5) {
    Usage();
    return -1;
  }

  const std::string coord_addr = argv[1];
  const size_t num_items = std::stoi(argv[2]);
  const size_t len_val = std::stoi(argv[3]);
  const size_t num_threads = std::stoi(argv[4]);
  assert(num_threads == 1);
  std::vector<Measurement> measurements(num_threads);
  std::vector<std::thread> workers;
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back(std::bind(Work, &measurements[i], coord_addr, num_items, len_val));
  }
  for (size_t i = 0; i < num_threads; ++i) {
    workers[i].join();
  }

#ifdef ENABLE_MEASUREMENT
  printf("%lu, %f\n", len_val, measurements[0].average_time());
#else
  for (auto& m : measurements) {
    m.Print();
  }
#endif
  return 0;
}

/*
static int function_test_main(int argc, const char* argv[]) {
  assert(argc >= 2);
  std::string coord_addr {argv[1]};
  try {
    Client c {coord_addr};
    client = &c;
    std::string author = "DDST-SJTU";
    std::string version = "0.1.0";
    c.Put("author", "DDST-SJTU");
    c.Put("version", "0.1.0");
    std::cout << "author: "  << c.Get("author")  << std::endl;
    std::cout << "version: " << c.Get("version") << std::endl;

    c.Put("author", "wgtdkp");
    c.Put("version", "0.2.0");
    std::cout << "author: "  << c.Get("author")  << std::endl;
    std::cout << "version: " << c.Get("version") << std::endl;

    c.Del("author");
    c.Del("version");
    std::cout << "author: "  << c.Get("author")  << std::endl;
    std::cout << "version: " << c.Get("version") << std::endl;
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("initialize infiniband devices failed");
  }
  return 0;
}
*/

int main(int argc, const char* argv[]) {
  signal(SIGINT, SigInt);
  //return function_test_main(argc, argv);
  return bench_main(argc, argv);
}
