#include "client.h"

#include <iostream>

using namespace nvds;

int main() {
  Client c {"192.168.99.11"};
  c.Put("AUTHOR", "SJTU-DDST");
  c.Put("VERSION", "0.0.1");

  const auto& a = c.Get("AUTHOR");
  const auto& v = c.Get("VERSION");

  std::cout << "AUTHOR: " << a << std::endl;
  std::cout << "VERSION: " << v << std::endl;

  return 0;
}
