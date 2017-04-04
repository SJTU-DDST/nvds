#include "server.h"

#include "config.h"
#include "json.hpp"

namespace nvds {

using json = nlohmann::json;

Server::Server(NVMPtr<NVMDevice> nvm, uint64_t nvm_size)
    : BasicServer(Config::server_port()),
      nvm_size_(nvm_size), nvm_(nvm) {}

void Server::Run() {
  // TODO(wgtdkp): initializations

  //Accept(std::bind(&Server::HandleMessage, this, std::placeholders::_1));
  RunService();
}

bool Server::Join() {
  using boost::asio::ip::tcp;
  tcp::resolver resolver(tcp_service_);
  tcp::resolver::query query(Config::coord_addr(),
                             std::to_string(Config::coord_port()));
  auto ep = resolver.resolve(query);
  try {
    boost::asio::connect(conn_sock_, ep);
    //NVDS_LOG("connected to coordinator: %s: %" PRIu16,
    //         Config::coord_addr().c_str(), Config::coord_port());
    // TODO(wgtdkp): construct message of join request
    Message::Header header {Message::SenderType::SERVER,
                            Message::Type::REQ_JOIN, 0};
    Message msg(header, json({{"size", nvm_size_}}).dump());
    
  } catch (boost::system::system_error& err) {
    NVDS_ERR("connect to coordinator: %s: %" PRIu16 "failed",
             Config::coord_addr().c_str(), Config::coord_port());
    return false;
  }
  return true;
}

void Server::Leave() {

}

void Server::Listening() {

}

void Server::HandleMessage(Session& session) {

}

} // namespace nvds
