#ifndef _NVDS_OBJECT_H_
#define _NVDS_OBJECT_H_

#include <string>

namespace nvds {

uint64_t Hash(const std::string& key);

class Object {
public:
  Object(const std::string& key, const std::string& value)
      : key_hash_(Hash(key)), key_(key), value_(value) {}
  ~Object() {}

  uint64_t key_hash() const { return key_hash_; }
  std::string& key() { return key_; }
  const std::string& key() const { return key_; }
  std::string& value() { return value_; }
  const std::string& value() const { return value_; }

private:
  uint64_t key_hash_;
  std::string key_;
  std::string value_;
};

} // namespace nvds

#endif // _NVDS_OBJECT_H_
