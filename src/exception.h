#ifndef _NVDS_EXCEPTION_H_
#define _NVDS_EXCEPTION_H_

#include "common.h"
#include "source_location.h"

#include <cstring>
#include <exception>
#include <typeinfo>

namespace nvds {

std::string Demangle(const char* name);

class Exception : public std::exception {
 public:
  explicit Exception (const SourceLocation& loc)
      : msg_(""), err_no_(0), loc_(loc) {}
  Exception(const SourceLocation& loc, const std::string& msg)
      : msg_(msg), err_no_(0), loc_(loc) {}
  Exception(const SourceLocation& loc, int err_no)
      : msg_(strerror(err_no)), err_no_(err_no), loc_(loc) {}
  Exception(const SourceLocation& loc, const std::string& msg, int err_no)
      : msg_(msg + ": " + strerror(err_no)), err_no_(err_no), loc_(loc) {}
  virtual ~Exception() {}
  
  std::string ToString() const {
    auto ret = Demangle(typeid(*this).name()) + ": " +
               msg_ + ", thrown at " + loc_.ToString();
    if (err_no_ != 0) {
        ret += " [errno: " + std::to_string(err_no_) + "]";
    }
    return ret;
  }
  const std::string& msg() const { return msg_; }
  int err_no() const { return err_no_; }
  const SourceLocation& loc() const { return loc_; }

 private:
  std::string msg_;
  int err_no_;
  SourceLocation loc_;
};

} // namespace nvds

#endif // _NVDS_EXCEPTION_H_
