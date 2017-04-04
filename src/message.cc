#include "message.h"

namespace nvds {

Message::Message(const char* raw) {
  if (raw == nullptr)
    return;
  header_ = *reinterpret_cast<const Header*>(raw);
  raw += kHeaderSize;
  body_ = std::string(raw, body_len());
}

} // namespace nvds
