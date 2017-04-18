#include "client.h"

#include <iostream>

using namespace nvds;

int main() {
  try {
    Client c {"192.168.1.67"};
    c.Put("AUTHOR", "SJTU-DDST");
    c.Put("VERSION", "0.0.1");

    auto a = c.Get("AUTHOR");
    auto v = c.Get("VERSION");

    std::cout << "AUTHOR: "  << a << std::endl;
    std::cout << "VERSION: " << v << std::endl;
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("infiniband devices are needed");
  }
  return 0;
}
