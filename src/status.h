#ifndef _NVDS_STATUS_H_
#define _NVDS_STATUS_H_

#include "common.h"

namespace nvds {

  enum class Status : uint8_t {
    OK, ERROR, NO_MEM,
  };

} // namespace nvds

#endif // _NVDS_STATUS_H_
