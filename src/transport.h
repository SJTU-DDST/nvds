#ifndef _NVDS_TRANSPORT_H_
#define _NVDS_TRANSPORT_H_

#include "exception.h"

namespace nvds {
class Message;

struct TransportException : public Exception {
  explicit TransportException(const SourceLocation& loc)
      : Exception(loc) {}
  TransportException(const SourceLocation& loc, const std::string& msg)
      : Exception(loc, msg) {}
  TransportException(const SourceLocation& loc, int err_no)
      : Exception(loc, err_no) {}
  TransportException(const SourceLocation& loc,
                     const std::string& msg, int err_no)
      : Exception(loc, msg, err_no) {}
};

class Transport {
 public:
  static const uint32_t kMaxRPCLen = (1 << 23) + 200;

  class ServerRPC {

  };

};

} // namespace nvds

#endif // _NVDS_TRANSPORT_H_
