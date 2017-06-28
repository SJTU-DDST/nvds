/*
 * Startup module of server.
 */

#include "server.h"

#include <algorithm>

using namespace nvds;

static Server* server;

static void SigInt(int signo) {
#ifdef ENABLE_MEASUREMENT
  double total = server->alloc_measurement.average_time() +
                 server->sync_measurement.average_time() +
                 server->thread_measurement.average_time() +
                 server->send_measurement.average_time()/* +
                 server->recv_measurement.average_time()*/;
  printf("%f, %f, %f, %f, %f\n",
         server->alloc_measurement.average_time(),
         server->sync_measurement.average_time(),
         server->thread_measurement.average_time(),
         server->send_measurement.average_time(),
         /*server->recv_measurement.average_time(),*/ total);
#else
  std::cout << std::endl << "num_recv: " << server->num_recv() << std::endl;
  std::cout << "alloc measurement: " << std::endl;
  server->alloc_measurement.Print();
  std::cout << std::endl;

  std::cout << "sync measurement: " << std::endl;
  server->sync_measurement.Print();
  std::cout << std::endl;

  std::cout << "thread measurement: " << std::endl;
  server->thread_measurement.Print();
  std::cout << std::endl;

  std::cout << "send measurement: " << std::endl;
  server->send_measurement.Print();
  std::cout << std::endl;

  //std::cout << "recv measurement: " << std::endl;
  //server->recv_measurement.Print();
  //std::cout << std::endl;

  double total = server->alloc_measurement.average_time() +
                 server->sync_measurement.average_time() +
                 server->thread_measurement.average_time() +
                 server->send_measurement.average_time();
                 //server->recv_measurement.average_time();
  std::cout << "total time: " << total << std::endl;
#endif
  std::cout << std::flush;
  exit(0);
}

static void Usage(int argc, const char* argv[]) {
    std::cout << "Usage:" << std::endl
              << "    " << argv[0] << " <port> <coord addr>" << std::endl;
}

int main(int argc, const char* argv[]) {
  signal(SIGINT, SigInt);
  // -2. Argument parsing.
  if (argc < 3) {
    Usage(argc, argv);
    return -1;
  }

  uint16_t server_port = std::stoi(argv[1]);
  std::string coord_addr = argv[2];

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
    if (!s.Join(coord_addr)) {
      NVDS_ERR("join cluster failed");
      return -1;
    }

    // Step 2: get the arrangement from coordinator, contacting servers
    //         that will be affected, performing data migration.

    // Step 3: acknowledge the coordinator of complemention

    // Step 4: serving request
  #ifndef ENABLE_MEASUREMENT
    NVDS_LOG("Server startup");
    NVDS_LOG("Listening at: %u", server_port);
    NVDS_LOG("......");
  #endif
    
    s.Run();
  } catch (boost::system::system_error& e) {
    NVDS_ERR(e.what());
  } catch (TransportException& e) {
    NVDS_ERR(e.msg().c_str());
    NVDS_LOG("initialize infiniband devices failed");
  }
  return 0;
}
