#ifndef _NVDS_TABLET_H_
#define _NVDS_TABLET_H_

#include <cstdint>

namespace nvds {

class Tablet {
public:
  Tablet(uint64_t begin, uint64_t end)
      : begin_(begin), end_(end) {}
  ~Tablet() {}

  uint64_t begin() const { return begin_; }
  uint64_t end() const { return end_; }

private:
  // The hash key begin of the tablet
  uint64_t begin_;
  // The hash key end of the tablet
  uint64_t end_;
};

} // namespace nvds

#endif // _NVDS_TABLET_H_
