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

#define PACKED(s) s __attribute__((packed))

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)        \
  TypeName(const TypeName&) = delete;             \
  TypeName& operator=(const TypeName&) = delete;
#endif

#define NVDS_ERR(format, args...) {         \
  std::cerr << "error: "                    \
            << nvds::Format(format, ##args) \
            << std::endl;                   \
}

#define NVDS_LOG(format, args...) {                       \
  std::cout << nvds::Format(format, ##args) << std::endl; \
}

namespace nvds {

using ServerId = uint32_t;
using TabletId = uint32_t;

/*
 * Cluster configuration
 */
// Configurable
static const uint32_t kNumReplicas = 1;
static const uint32_t kNumServers = 2;
static const uint32_t kNumTabletsPerServer = 1;

static const uint16_t kCoordPort = 9090;
static const uint32_t kMaxItemSize = 1024;
static const uint32_t kNumTabletAndBackupsPerServer = kNumTabletsPerServer * (1 + kNumReplicas);
static const uint32_t kNumTablets = kNumTabletsPerServer * kNumServers;
static const uint32_t kNumTabletAndBackups = kNumTabletAndBackupsPerServer * kNumServers;
static_assert(kNumTablets % kNumServers == 0,
              "`kNumTablets` cannot be divisible by `kNumServers`");

/*
 * Infiniband configuration
 */
static const uint32_t kSendBufSize = 1024 + 128;
static const uint32_t kRecvBufSize = 1024 * 2 + 128;
static const uint32_t kIBUDPadding = 40;

std::string Format(const char* format, ...);
std::string Demangle(const char* name);

static inline void memset_volatile(volatile char* ptr, char v, size_t size) {
  auto end = ptr + size;
  while (ptr != end) {
    *ptr++ = v;
  }
}

} // namespace nvds

#endif // _NVDS_COMMON_H_
