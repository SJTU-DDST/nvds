#include "server.h"

#include "config.h"

using namespace nvds;

static uint64_t kNVMSize = 512 * 1024 * 1024;

static void Usage(int argc, const char* argv[]) {
    std::cout << "Usage:" << std::endl
              << "    " << argv[0] << " config_file" << std::endl;
}

int main(int argc, const char* argv[]) {
  // -2. Argument parsing.
  if (argc < 2) {
    Usage(argc, argv);
    return -1;
  }

  // -1. Load config file.
  Config::GetInst()->Load(argv[1]);

  // Step 0, self initialization, including formatting nvm storage.
  // DRAM emulated NVM.
  NVMPtr<NVMDevice> nvm(static_cast<NVMDevice*>(malloc(kNVMSize)));
  if (nvm == nullptr) {
    NVDS_ERR("acquire nvm failed: size = %PRIu64\n", kNVMSize);
    return -1;
  }

  Server s(nvm, kNVMSize);

  // Step 1: request to the coordinator for joining in.

  // Step 2: get the arrangement from coordinator, contacting servers
  //         that will be affected, performing data migration.

  // Step 3: acknowledge the coordinator of complemention

  // Step 4: serving request
  
  return 0;
}
