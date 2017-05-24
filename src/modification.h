#ifndef _NVDS_MODIFICATION_H_
#define _NVDS_MODIFICATION_H_

#include "common.h"

namespace nvds {

struct Modification {
  uint32_t des;
  uint32_t len;
  uint64_t src;
  Modification(uint32_t des, uint64_t src, uint32_t len)
      : des(des), len(len), src(src) {}
  // For STL
  Modification() : des(0), len(0), src(0) {}
  bool operator<(const Modification& other) const {
    return des < other.des;
  }
  // DEBUG
  void Print() const {
    std::cout << "des: " << des << "; ";
    std::cout << "len: " << len << "; ";
    std::cout << "src: " << src << std::endl;
  }
};
using ModificationList = std::vector<Modification>;

struct Position {
  uint32_t offset;
  uint32_t len;
  Position() = delete;
};

struct ModificationLog {
  uint32_t cnt;
  Position positions[0];
  ModificationLog() = delete;
};

} // namespace nvds

#endif // _NVDS_MODIFICATION_H_
