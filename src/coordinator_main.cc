/*
 * Startup module of coordinator. For a storage cluster,
 * it's the coordinator that starts up first, load configs
 * from the config file, 
 */

#include "coordinator.h"

using namespace nvds;

/*
static void Usage(int argc, const char* argv[]) {
  std::cout << "Usage:" << std::endl
            << "    " << argv[0] << " <config_file>" << std::endl;
}
*/

static std::string GetLocalAddr() {
  using boost::asio::ip::tcp;
  boost::asio::io_service io_service;
  tcp::resolver resolver(io_service);
  tcp::resolver::query query(boost::asio::ip::host_name(), "");
  auto iter = resolver.resolve(query);
  decltype(iter) end;
  while (iter != end) {
    tcp::endpoint ep = *iter++;
    return ep.address().to_string();
  }
  assert(false);
  return "";
}

int main(int argc, const char* argv[]) {
  // -1. Argument parsing
  //if (argc < 2) {
  //  Usage(argc, argv);
  //  return -1;
  //}

  // 1. Self initialization
  Coordinator coordinator;
  
  // 2. Listening for joining request from nodes and request from clients
  NVDS_LOG("Coordinator at: %s", GetLocalAddr().c_str());
  NVDS_LOG("Listening at: %u", kCoordPort);
  NVDS_LOG("......");

  coordinator.Run();

  return 0;
}
