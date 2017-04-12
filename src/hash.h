#ifndef _NVDS_HASH_H_
#define _NVDS_HASH_H_

namespace nvds {

using KeyHash = uint64_t;
static const KeyHash kMaxKeyHash = static_cast<KeyHash>(-1);

struct KeyHashRange {
  KeyHash begin;
  KeyHash end;
};

} // namespace nvds

#endif // _NVDS_HASH_H_
