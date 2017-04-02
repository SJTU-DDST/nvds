#include "server.h"

namespace nvds {

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
