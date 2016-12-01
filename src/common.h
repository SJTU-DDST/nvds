#ifndef _NVDS_COMMON_H_
#define _NVDS_COMMON_H_

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace nvds {

#define REPLICA_NUM (3)

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)          \
    TypeName(const TypeName&) = delete;             \
    TypeName& operator=(const TypeName&) = delete;
#endif

std::string Format(const char* format, ...);
std::string demangle(const char* name);


} // namespace nvds

#endif // _NVDS_COMMON_H_
