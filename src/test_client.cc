/*
 * Compile: g++ -std=c++11 -Wall -g test_client.cc -lnvds -lboost_system -lboost_thread -pthread -libverbs
 */

#include "nvds/client.h"

#include <iostream>
#include <string>

int main() {
  try {
    nvds::Client c {"192.168.99.14"};
    c.Put("hello", "world");
    assert(!c.Add("hello", "NVDS"));
    std::cout << "hello: " << c.Get("hello") << std::endl;
    c.Del("hello");
    assert(c.Add("hello", "world"));
    c.Del("hello");
    assert(c.Get("hello").size() == 0);
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (nvds::TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("initialize infiniband devices failed");
  }
  return 0;
}
