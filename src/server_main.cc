/*
 * Startup module of server.
 */

#include "server.h"
#include "config.h"

using namespace nvds;

static Server* server;

static void SigInt(int signo) {
  std::cout << std::endl << "num_recv: " << server->num_recv() << std::endl;
  std::cout << std::flush;
  exit(0);
}

static void Usage(int argc, const char* argv[]) {
    std::cout << "Usage:" << std::endl
              << "    " << argv[0] << " <port> <config_file>" << std::endl;
}

int main(int argc, const char* argv[]) {
  signal(SIGINT, SigInt);
  // -2. Argument parsing.
  if (argc < 3) {
    Usage(argc, argv);
    return -1;
  }

  uint16_t server_port = std::stoi(argv[1]);

  // -1. Load config file.
  Config::GetInst()->Load(argv[2]);

  // Step 0, self initialization, including formatting nvm storage.
  // DRAM emulated NVM.
  auto nvm = AcquireNVM<NVMDevice>(kNVMDeviceSize);
  if (nvm == nullptr) {
    NVDS_ERR("acquire nvm failed: size = %PRIu64", kNVMDeviceSize);
    return -1;
  }
  try {
    Server s(server_port, nvm, kNVMDeviceSize);
    server = &s;
    // Step 1: request to the coordinator for joining in.
    if (!s.Join()) {
      NVDS_ERR("join cluster failed");
      return -1;
    }

    // Step 2: get the arrangement from coordinator, contacting servers
    //         that will be affected, performing data migration.

    // Step 3: acknowledge the coordinator of complemention

    // Step 4: serving request
    NVDS_LOG("Server startup");
    NVDS_LOG("Listening at: %u", server_port);
    NVDS_LOG("......");
    
    s.Run();
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("initialize infiniband devices failed");
  }
  return 0;
}
