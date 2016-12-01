#ifndef _NVDS_EXCEPTION_H_
#define _NVDS_EXCEPTION_H_

#include "source_location.h"

#include <cstring>
#include <exception>
#include <typeinfo>

namespace nvds {

std::string demangle(const char* name);;

class Exception : public std::exception {
 public:
  explicit Exception (const SourceLocation& loc)
      : msg_(""), errno_(0), loc_(loc) {}
  Exception(const SourceLocation& loc, const std::string& msg)
      : msg_(msg), errno_(0), loc_(loc) {}
  Exception(const SourceLocation& loc, int err)
      : msg_(strerror(err)), errno_(err), loc_(loc) {}
  Exception(const SourceLocation& loc, const std::string& msg, int err)
      : msg_(msg + ": " + strerror(err)), errno_(err), loc_(loc) {}
  virtual ~Exception() {}
  
  std::string ToString() const {
    return demangle(typeid(*this).name()) + ": " + msg_ +
           ", thrown at " + loc_.ToString();
  }
  const std::string& msg() const { return msg_; }
  //int errno() const { return errno_; }
  const SourceLocation& loc() const { return loc_; }

 private:
  std::string msg_;
  int errno_;
  SourceLocation loc_;
};

} // namespace nvds

#endif // _NVDS_EXCEPTION_H_
