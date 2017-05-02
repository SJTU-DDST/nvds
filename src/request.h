#ifndef _NVDS_REQUEST_H_
#define _NVDS_REQUEST_H_

#include "common.h"

namespace nvds {

struct Request {
  enum class Type : uint8_t {
    PUT, GET, DEL
  };
  Type type;
  uint16_t key_len;
  uint16_t val_len;
  KeyHash key_hash;
  // Key data followed by value data
  char data[0];
 
  static Request* New(void* ptr, Type type, const std::string& key,
                      const std::string& val, KeyHash key_hash) {
    //auto buf = new char[sizeof(Request) + key.size() + val.size()];
    return new (ptr) Request(type, key, val, key_hash);
  }
  static void Del(const Request* r) {
    // Explicitly call destructor(only when pairing with placement new)
    r->~Request();
  }
  size_t Len() const { return sizeof(Request) + key_len + val_len; }
  char* Key() { return data; }
  const char* Key() const { return data; }
  char* Val() { return data + key_len; }
  const char* Val() const { return data + key_len; }

 private:
  Request(Type type, const std::string& key,
      const std::string& val, KeyHash key_hash)
      : type(type), key_len(key.size()),
        val_len(val.size()), key_hash(key_hash) {
    memcpy(data, key.c_str(), key_len);
    memcpy(data + key_len, val.c_str(), val_len);
  }
};

} // namespace nvds

#endif // _NVDS_SERVER_H_
