#ifndef _NVDS_COMMON_H_
#define _NVDS_COMMON_H_

#include "nvm.h"

#include <unistd.h>

#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace nvds {

#define PACKED(s) s __attribute__((packed))

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)        \
  TypeName(const TypeName&) = delete;             \
  TypeName& operator=(const TypeName&) = delete;
#endif

#define NVDS_ERR(format, args...) {     \
  std::cerr << "error: "                \
            << Format(format, ##args)   \
            << std::endl;               \
}

#define NVDS_LOG(format, args...) {                 \
  std::clog << Format(format, ##args) << std::endl; \
}

using ServerId = uint32_t;
using TabletId = uint32_t;
static const uint16_t kCoordPort = 9090;
static const uint16_t kServerPort = 7070;
static const uint32_t kMaxItemSize = 1024;
static const uint32_t kNumReplicas = 2;
static const uint32_t kNumServers = 1;
static const uint32_t kNumTablets = 64;
static_assert(kNumTablets % kNumServers == 0,
              "`kNumTablets` cannot be divisible by `kNumServers`");
static const uint32_t kNumTabletsPerServer = kNumTablets / kNumServers;

std::string Format(const char* format, ...);
std::string Demangle(const char* name);

} // namespace nvds

#endif // _NVDS_COMMON_H_
