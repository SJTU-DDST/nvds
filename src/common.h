#ifndef _NVDS_COMMON_H_
#define _NVDS_COMMON_H_

#include "nvm.h"

#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace nvds {

#define PACKED(s) s __attribute__((packed))

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)          \
    TypeName(const TypeName&) = delete;             \
    TypeName& operator=(const TypeName&) = delete;
#endif

#define NVDS_ERR(format, args...) {           \
  std::cerr << "error: "                      \
            << Format(format, args)           \
            << std::endl;                     \
}

#define NVDS_LOG(format, args...) {               \
  std::clog << Format(format, args) << std::endl; \
}

using KeyHash = uint64_t;
static const uint32_t kMaxItemSize = 1024 * 1024;
static const uint32_t kNumReplica = 2;

std::string Format(const char* format, ...);
std::string Demangle(const char* name);

} // namespace nvds

#endif // _NVDS_COMMON_H_
