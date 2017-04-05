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

  Accept(std::bind(&Coordinator::HandleRecvMessage, this,
                   std::placeholders::_1, std::placeholders::_2),
         std::bind(&Coordinator::HandleSendMessage, this,
                   std::placeholders::_1, std::placeholders::_2));
  RunService();
}

void Coordinator::HandleRecvMessage(Session& session,
                                    std::shared_ptr<Message> msg) {
  switch (msg->sender_type()) {
  case Message::SenderType::SERVER:
    HandleMessageFromServer(session, msg);
    break;
  case Message::SenderType::CLIENT:
    HandleMessageFromClient(session, msg);
    break;
  case Message::SenderType::COORDINATOR:
    assert(false);
  }
}

void Coordinator::HandleSendMessage(Session& session, std::shared_ptr<Message> msg) {
  // TODO(wgtdkp): implement
  assert(false);
}

void Coordinator::HandleMessageFromServer(Session& session, std::shared_ptr<Message> msg) {
  switch (msg->type()) {
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

void Coordinator::HandleMessageFromClient(Session& session, std::shared_ptr<Message> msg) {
  switch (msg->type()) {
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
