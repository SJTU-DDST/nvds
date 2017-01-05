#ifndef _NVDS_COMMON_H_
#define _NVDS_COMMON_H_

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace nvds {

#define PACKED(s) s __attribute__((packed))

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)          \
    TypeName(const TypeName&) = delete;             \
    TypeName& operator=(const TypeName&) = delete;
#endif

using KeyHash = uint64_t;
static const uint32_t kMaxItemSize = 1024 * 1024;
static const uint32_t kNumReplica = 3;

std::string Format(const char* format, ...);
std::string demangle(const char* name);

} // namespace nvds

#endif // _NVDS_COMMON_H_
