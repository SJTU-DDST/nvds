#ifndef _NVDS_RESPONSE_H_
#define _NVDS_RESPONSE_H_

#include "common.h"
#include "infiniband.h"
#include "request.h"

namespace nvds {

struct Response {
  enum class Status : uint8_t {
    OK, ERROR, NO_MEM,
  };
  Request::Type type;
  Status status;
  uint16_t val_len;
  char val[0];

  static Response* New(Infiniband::Buffer* b,
                       Request::Type type, Status status) {
    return new (b->buf) Response(type, status);
  }
  static void Del(const Response* r) {
    r->~Response();
  }

 private:
  Response(Request::Type t, Status s)
      : type(t), status(s), val_len(0) {
  }
};

} // namespace nvds

#endif // _NVDS_RESPONSE_H_
