/*
 * Startup module for coordinator. For a storage cluster,
 * it's the coordinator that starts up first, load configs
 * from the config file, 
 */

#include "coordinator.h"
#include "config.h"
#include "json.hpp"

using json = nlohmann::json;
using namespace nvds;

static void Usage(int argc, const char* argv[]) {
  std::cout << "Usage:" << std::endl
            << "    " << argv[0] << " config_file" << std::endl;
}

int main(int argc, const char* argv[]) {
  // -1. Argument parsing
  if (argc < 2) {
    Usage(argc, argv);
    return -1;
  }

  // 0. Load config file
  Config::GetInst()->Load(argv[1]);

  // 1. Self initialization


  // 2. Listening for joining request from nodes and request from clients
  

  return 0;
}
