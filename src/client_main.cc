#include "client.h"

#include <iostream>

using namespace nvds;

void Usage() {
  std::cout << "usage: " << std::endl;
  std::cout << "client  <coordinator_addr>" << std::endl;
}

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    Usage();
    return -1;
  }

  const std::string coord_addr = argv[1];
  try {
    Client c {coord_addr};
    c.Put("AUTHOR", "SJTU-DDST");
    c.Put("AUTHOR", "SJTU-DDST");
    //c.Put("VERSION", "0.0.1");

    auto a = c.Get("AUTHOR");
    auto v = c.Get("VERSION");

    std::cout << "AUTHOR: "  << a << std::endl;
    std::cout << "VERSION: " << v << std::endl;
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("initialize infiniband devices failed");
  }
  return 0;
}
