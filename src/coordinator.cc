#include "coordinator.h"

#include "config.h"
#include "message.h"
#include "session.h"

namespace nvds {

Coordinator::Coordinator()
    : BasicServer(Config::coord_port()) {

}

void Coordinator::Run() {
  // TODO(wgtdkp): initializations

  Accept(std::bind(&Coordinator::HandleMessage, this, std::placeholders::_1));
  RunService();
}

void Coordinator::HandleMessage(Session& session) {
  switch (session.msg().sender_type()) {
  case Message::SenderType::SERVER:
    HandleMessageFromServer(session);
    break;
  case Message::SenderType::CLIENT:
    HandleMessageFromClient(session);
    break;
  case Message::SenderType::COORDINATOR:
    assert(false);
  }
}

void Coordinator::HandleMessageFromServer(Session& session) {
  const auto& msg = session.msg();
  switch (msg.type()) {
  case Message::Type::REQ_JOIN:
    break;
  case Message::Type::REQ_LEAVE:
    break;
  case Message::Type::ACK_REJECT:
    break;
  case Message::Type::ACK_ERROR:
    break;
  case Message::Type::ACK_OK:
    break;
  default:
    assert(false);
  }
}

void Coordinator::HandleMessageFromClient(Session& session) {
  const auto& msg = session.msg();
  switch (msg.type()) {
  case Message::Type::REQ_JOIN:
    break;
  case Message::Type::REQ_LEAVE:
    break;
  case Message::Type::ACK_REJECT:
    break;
  case Message::Type::ACK_ERROR:
    break;
  case Message::Type::ACK_OK:
    break;
  default:
    assert(false);
  }
}

} // namespace nvds
