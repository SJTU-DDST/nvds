#include "server.h"

#include "config.h"

namespace nvds {

Server::Server(NVMPtr<NVMDevice> nvm, uint64_t nvm_size)
    : BasicServer(Config::server_port()),
      nvm_size_(nvm_size), nvm_(nvm) {}

void Server::Run() {
  // TODO(wgtdkp): initializations

  Accept(std::bind(&Server::HandleMessage, this, std::placeholders::_1));
  RunService();
}

bool Server::Join() {
  
  return true;
}

void Server::Leave() {

}

void Server::Listening() {

}

void Server::HandleMessage(Session& session) {

}

} // namespace nvds
