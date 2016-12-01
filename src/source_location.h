#ifndef _NVDS_SOURCE_LOCATION_H_
#define _NVDS_SOURCE_LOCATION_H_

#include <cstdint>
#include <string>

namespace nvds {

#define HERE  (SourceLocation {__FILE__, __func__, __LINE__})

struct SourceLocation {
  std::string file;
  std::string func;
  uint32_t line;

  std::string ToString() const {
    return file + ":" + func + ":" + std::to_string(line);
  }
};

} // namespace nvds

#endif // _NVDS_SOURCE_LOCATION_H_