#ifndef _NVDS_RESPONSE_H_
#define _NVDS_RESPONSE_H_

#include "common.h"
#include "infiniband.h"
#include "request.h"

namespace nvds {

struct Response {
  using Type = Request::Type;
  enum class Status : uint8_t {
    OK, ERROR, NO_MEM,
  };
  Type type;
  Status status;
  uint16_t val_len;
  char val[0];

  static Response* New(Infiniband::Buffer* b, Type type, Status status) {
    return new (b->buf) Response(type, status);
  }
  static void Del(const Response* r) {
    r->~Response();
  }
  uint32_t Len() const {
    return sizeof(Response) + (type == Type::GET ? val_len : 0);
  }
  void Print() const {
    std::cout << "type: " << (type == Type::GET ? "GET" : type == Type::PUT ? "PUT" : "DEL") << std::endl;
    std::cout << "status: " << (status == Status::OK ? "OK" : status == Status::ERROR ? "ERROR" : "NO_MEM") << std::endl;
    std::cout << "val_len: " << val_len << std::endl;
    std::cout << "val: ";
    for (size_t i = 0; i < val_len; ++i)
      std::cout << val[i];
    std::cout << std::endl;
  }
 private:
  Response(Request::Type t, Status s)
      : type(t), status(s), val_len(0) {
  }
};

} // namespace nvds

#endif // _NVDS_RESPONSE_H_
