#ifndef _NVDS_MESSAGE_H_
#define _NVDS_MESSAGE_H_

#include "common.h"

namespace nvds {

/*
 The message body is in json format, it looks like:
 {
   "name-0": value,
   "name-1": value
 }
 
 0. REQ_JOIN :
 {
   "size": int,
   "infiniband": {
     "ib_port": int,
     "lid": int,
     "pqn": int,
   }
 }
 1. RES_JOIN :
 {
   "id": int,
   "backups_id": array[int],
   "key_ranges": array[array[int]]
 }
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

  Message() {}
  Message(Header header, std::string body)
      : header_(std::move(header)), body_(std::move(body)) {
    header_.body_len = body_.size();
  }
  //explicit Message(const char* raw=nullptr);
  ~Message() {}

  static const size_t kHeaderSize = sizeof(Header);

  const Header& header() const { return header_; }
  Header& header() { return header_; }
  const std::string& body() const { return body_; }
  std::string& body() { return body_; }
  
  SenderType sender_type() const { return header_.sender_type; }
  Type type() const { return header_.type; }
  uint32_t body_len() const { return header_.body_len; }
  
 private:
  Header header_;
  std::string body_;
};

} // namespace nvds

#endif // _NVDS_MESSAGE_
