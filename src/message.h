#ifndef _NVDS_MESSAGE_H_
#define _NVDS_MESSAGE_H_

#include "common.h"

namespace nvds {

class Message {
 public:
  struct Format;

  Message(Format* msg=nullptr) : msg_(msg) {}

  enum class SenderType : uint8_t {
    COORDINATOR,
    SERVER,
    CLIENT,
  };

  enum class Type : uint8_t {
    REQ_JOIN,         // server/client ---> coordinator
    REQ_LEAVE,        // server/client ---> coordinator
    RES_CLUSTER_INFO, // coordinator   ---> client
    QP_INFO_EXCH,     // server/client ---> server
    ACK_REJECT,       // Acknowledgement: reject
    ACK_ERROR,        // Acknowledgement: error
    ACK_OK,           // Acknowledgement: ok
  };

  PACKED(struct Header {
    SenderType sender_type;
    Type type;
    uint32_t length;
  });

  PACKED(struct Format {
    Header header;
    uint8_t body[0];
  });

  static const size_t kHeaderLen = sizeof(Header);

  SenderType sender_type() const { return msg_->header.sender_type; }
  void set_sender_type(const SenderType& sender_type) {
    msg_->header.sender_type = sender_type;
  }
  uint32_t body_len() const { return msg_->header.length; }
 private:
  Format* msg_;
};

} // namespace nvds

#endif // _NVDS_MESSAGE_
