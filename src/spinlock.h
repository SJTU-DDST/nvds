#ifndef _NVDS_SPINLOCK_H_
#define _NVDS_SPINLOCK_H_

#include "common.h"

#include <atomic>

namespace nvds {

class Spinlock {
 public:
  Spinlock() = default;
  DISALLOW_COPY_AND_ASSIGN(Spinlock);
  void lock() {
    while (flag_.test_and_set(std::memory_order_acquire)) {}
  }
  void unlock() {
    flag_.clear(std::memory_order_release);
  }

 private:
  std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

} // namespace nvds

#endif // _NVDS_SPINLOCK_H_
