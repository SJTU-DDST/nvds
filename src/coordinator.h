#ifndef _NVDS_COORDINATOR_H_
#define _NVDS_COORDINATOR_H_

#include "basic_server.h"
#include "index.h"

#include <boost/asio.hpp>

namespace nvds {

class Session;

/*
 * Coordinator do not serve any frequent reuqests from client,
 * like the get/put operation. It accepts various messages,
 * controls server recovery/joining/removing and failure
 * detection. It maintains the metadata of the whole cluster.
 *
 * At any given time, there is at most one active coordinator.
 * But several standby coordinators are preparing to take in charge
 * in case of the primary coordinator is down.
 */
class Coordinator : public BasicServer {
 public:
  Coordinator();
  ~Coordinator() {}
  DISALLOW_COPY_AND_ASSIGN(Coordinator);

  uint64_t total_storage() const { return total_storage_; }

  IndexManager& index_manager() { return index_manager_; }
  const IndexManager& index_manager() const { return index_manager_; }

  void Run() override;

 private:
  void HandleRecvMessage(std::shared_ptr<Session> session,
                         std::shared_ptr<Message> msg);
  void HandleSendMessage(std::shared_ptr<Session> session,
                         std::shared_ptr<Message> msg);
  void HandleMessageFromServer(std::shared_ptr<Session> session,
                               std::shared_ptr<Message> msg);
  void HandleServerRequestJoin(std::shared_ptr<Session> session,
                               std::shared_ptr<Message> req);
  void ResponseAllJoins();
  void HandleMessageFromClient(std::shared_ptr<Session> session,
                               std::shared_ptr<Message> msg);
  void HandleClientRequestJoin(std::shared_ptr<Session> session,
                               std::shared_ptr<Message> msg);

 private:
  uint32_t num_servers_ = 0;
  uint64_t total_storage_ = 0;

  IndexManager index_manager_;
  std::vector<std::shared_ptr<Session>> sessions_;
};

} // namespace nvds

#endif // NVDS_COORDINATOR_H_
