#ifndef _NVDS_COORDINATOR_H_
#define _NVDS_COORDINATOR_H_

#include "index.h"

namespace nvds {

/*
 * At any given time, there is at most one active coordinator
 * But several standby coordinator preparing to take in charge
 */
class Coordinator {
public:
  Coordinator() {}
  ~Coordinator() {}

  IndexManager& index_manager() { return index_manager_; }
  const IndexManager& index_manager() const { return index_manager_; }  

private:
  IndexManager index_manager_;
};

} // namespace nvds

#endif
