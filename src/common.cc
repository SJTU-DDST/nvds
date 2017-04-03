#include "common.h"

#include <boost/asio.hpp>
#include <cstdarg>
#include <cxxabi.h>

namespace nvds {

static std::string VFormat(const char* format, va_list ap);

/// A safe version of sprintf.
std::string Format(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  const auto& s = VFormat(format, ap);
  va_end(ap);
  return s;
}

/// A safe version of vprintf.
static std::string VFormat(const char* format, va_list ap) {
  std::string s;

  // We're not really sure how big of a buffer will be necessary.
  // Try 1K, if not the return value will tell us how much is necessary.
  int bufSize = 1024;
  while (true) {
    char buf[bufSize];
    // vsnprintf trashes the va_list, so copy it first
    va_list aq;
    __va_copy(aq, ap);
    int r = vsnprintf(buf, bufSize, format, aq);
    assert(r >= 0); // old glibc versions returned -1
    if (r < bufSize) {
      s = buf;
      break;
    }
    bufSize = r + 1;
  }
  return s;
}

std::string Demangle(const char* name) {
  int status;
  char* res = abi::__cxa_demangle(name, NULL, NULL, &status);
  if (status != 0) {
    return name;
  }
  std::string ret(res);
  free(res);
  return ret;
}

} // namespace nvds
