#ifndef _NVDS_MESSAGE_H_
#define _NVDS_MESSAGE_H_

#include "common.h"
#include "infiniband.h"
#include "json.hpp"

namespace nvds {

class IndexManager;

/*
 The message body is in json format, it looks like:
 {
   "name-0": value,
   "name-1": value
 }
 
 0. REQ_JOIN :
 {
   "size": int,
   "ib_addr": {
     "ib_port": int,
     "lid": int,
     "pqn": int,
   },
   "tablets_vaddr": [int],
   "tablets_rkey": [int]
 }
 1. RES_JOIN :
 {
   "id": int,
   "index_manager": IndexManager
 }
 */

/*
struct Backup {
  ServerId server_id;
  TabletId tablet_id;
  bool operator==(const Backup& other) const {
    return server_id == other.server_id &&
           tablet_id == other.tablet_id;
  }
  bool operator!=(const Backup& other) const {
    return !(*this == other);
  }
};
using Master = Backup;
*/

struct TabletInfo {
  TabletId id;
  ServerId server_id;
  bool is_backup;
  std::array<Infiniband::QueuePairInfo, kNumReplicas> qpis;
  union {
    // If `is_backup` == true,
    // `master` is the master tablet of this backup tablet
    TabletId master;
    // Else, `backups` is the backups of this makster tablet
    std::array<TabletId, kNumReplicas> backups;
  };
  bool operator==(const TabletInfo& other) const {
    return id == other.id &&
           server_id == other.server_id &&
           is_backup == other.is_backup;
  }
  bool operator!=(const TabletInfo& other) const {
    return !(*this == other);
  }
  // DEBUG
  void Print() const;
};

/* Json representation
{
  "id": int,
  "active": bool,
  "addr": string,
  "ib_addr": {
    "ib_port": int,
    "lid": int,
    "qpn": int,
  }
  "tablets": array[TabletInfo],
}
 */
struct ServerInfo {
  ServerId id;
  bool active;
  std::string addr; // Ip address
  Infiniband::Address ib_addr; // Infiniband address
  std::array<TabletId, kNumTabletAndBackupsPerServer> tablets;
};

class Message {
 public:
  enum class SenderType : uint8_t {
    COORDINATOR,
    SERVER,
    CLIENT,
  };

  enum class Type : uint8_t {
    REQ_JOIN,         // erver/client ---> coordinator
    REQ_LEAVE,        // server/client ---> coordinator
    RES_JOIN,         // coordinator   ---> server
    RES_CLUSTER_INFO, // coordinator   ---> client
    QP_INFO_EXCH,     // server/client ---> server
    ACK_REJECT,       // acknowledgement: reject
    ACK_ERROR,        // aknowledgement: error
    ACK_OK,           // acknowledgement: ok
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

void to_json(nlohmann::json& j, const Infiniband::Address& ia);
void from_json(const nlohmann::json& j, Infiniband::Address& ia);
void to_json(nlohmann::json& j, const Infiniband::QueuePairInfo& qpi);
void from_json(const nlohmann::json& j, Infiniband::QueuePairInfo& qpi);
void to_json(nlohmann::json& j, const TabletInfo& ti);
void from_json(const nlohmann::json& j, TabletInfo& ti);
void to_json(nlohmann::json& j, const ServerInfo& si);
void from_json(const nlohmann::json& j, ServerInfo& si);
void to_json(nlohmann::json& j, const IndexManager& im);
void from_json(const nlohmann::json& j, IndexManager& im);

} // namespace nvds

#endif // _NVDS_MESSAGE_
