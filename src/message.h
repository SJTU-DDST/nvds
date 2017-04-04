#ifndef _NVDS_MESSAGE_H_
#define _NVDS_MESSAGE_H_

#include "common.h"

namespace nvds {

/*
 * The message body is in json format, it looks like:
 * {
 *   "name-0": value,
 *   "name-1": value
 * }
 * 
 * 0. REQ_JOIN :
 * {
 *   "size": int
 * }
 */

class Message {
 public:
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
    uint32_t body_len;
  });

  Message(const Header& header, const std::string& body="")
      : header_(header), body_(body) {
    header_.body_len = body_.size();
  }
  explicit Message(const char* raw=nullptr);
  ~Message() {}

  static const size_t kHeaderSize = sizeof(Header);

  SenderType sender_type() const { return header_.sender_type; }
  Type type() const { return header_.type; }
  uint32_t body_len() const { return header_.body_len; }
  const std::string& body() const { return body_; }
  void AppendBody(const std::string& body) {
    header_.body_len += body.size();    
    body_ += body;
  }

 private:
  Header header_;
  std::string body_;
};

} // namespace nvds

#endif // _NVDS_MESSAGE_
