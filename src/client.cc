#include "client.h"

namespace nvds {

Client::Status Client::Connect(const std::string& coordinator_addr) {
  return Status::OK;
}

Client::Status Client::Close() {
  return Status::OK;
}

std::string Client::Get(const std::string& key) {
  return "";
}

Client::Status Client::Put(const std::string& key, const std::string& value) {
  return Status::OK;
}

Client::Status Client::Delete(const std::string& key) {
  return Status::OK;
}

}
