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
  // Key data followed by value data
  char data[0];
 
  static Request* New(Type type, const std::string& key,
                      const std::string& val) {
    auto buf = new char[sizeof(Request) + key.size() + val.size()];
    return new (buf) Request(type, key, val);
  }
  static void Del(const Request* r) {
    // Explicitly call destructor(only when pairing with placement new)
    r->~Request();
    auto buf = reinterpret_cast<const char*>(r);
    delete[] buf;
  }
  size_t Len() const { return sizeof(Request) + key_len + val_len; }

 private:
  Request(Type type, const std::string& key, const std::string& val)
      : type(type), key_len(key.size()), val_len(val.size()) {
    memcpy(data, key.c_str(), key_len);
    memcpy(data + key_len, val.c_str(), val_len);
  }
};

} // namespace nvds

#endif // _NVDS_SERVER_H_
