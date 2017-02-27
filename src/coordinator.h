#ifndef _NVDS_COORDINATOR_H_
#define _NVDS_COORDINATOR_H_

#include "index.h"

namespace nvds {

/*
 * Coordinator do not serve any frequent reuqests, like
 * the get/put operation. It accepts various message,
 * controls server recovery/joining/removing and failure
 * detection. It maintains the metadata of the whole cluster.
 *
 * At any given time, there is at most one active coordinator.
 * But several standby coordinators are preparing to take in charge.
 */
class Coordinator {
 public:
  Coordinator() {}
  ~Coordinator() {}

  // Listening request

  IndexManager& index_manager() { return index_manager_; }
  const IndexManager& index_manager() const { return index_manager_; }

 private:
  IndexManager index_manager_;
};

} // namespace nvds

#endif // NVDS_COORDINATOR_H_
