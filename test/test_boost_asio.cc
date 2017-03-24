#include <bits/stdc++.h>
#include <boost/asio.hpp>

using namespace boost::asio;

int main() {
  io_service io_service;
  ip::tcp::resolver resolver(io_service);
  ip::tcp::resolver::query query("www.wgtdkp.com", "http");
  auto iter = resolver.resolve(query);
  decltype(iter) end;
  while (iter != end) {
    ip::tcp::endpoint ep = *iter++;
    std::cout << ep << std::endl;
  }
  return 0;
}
