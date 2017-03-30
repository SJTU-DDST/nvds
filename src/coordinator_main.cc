/*
 * Startup module of coordinator. For a storage cluster,
 * it's the coordinator that starts up first, load configs
 * from the config file, 
 */

#include "coordinator.h"
#include "config.h"

using namespace nvds;

static void Usage(int argc, const char* argv[]) {
  std::cout << "Usage:" << std::endl
            << "    " << argv[0] << " <config_file>" << std::endl;
}

int main(int argc, const char* argv[]) {
  // -1. Argument parsing
  if (argc < 2) {
    Usage(argc, argv);
    return -1;
  }

  // 0. Load config file
  Config::GetInst()->Load(argv[1]);

  NVDS_LOG("Coordinator at: %s", Config::coord_addr().c_str());
  NVDS_LOG("Listening at: %u", Config::coord_port());
  NVDS_LOG("......");

  // 1. Self initialization
  Coordinator coordinator;
  
  // 2. Listening for joining request from nodes and request from clients
  coordinator.Run();

  return 0;
}
