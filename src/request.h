#ifndef _NVDS_REQUEST_H_
#define _NVDS_REQUEST_H_

#include "common.h"
#include "infiniband.h"

namespace nvds {

struct Request {
  enum class Type : uint8_t {
    PUT, GET, DEL
  };
  Type type;
  uint16_t key_len;
  uint16_t val_len;
  KeyHash key_hash;
  // TODO(wgtdkp):
  //uint64_t id;
  // Key data followed by value data
  char data[0];
 
  static Request* New(Infiniband::Buffer* b, Type type,
                      const char* key, size_t key_len,
                      const char* val, size_t val_len, KeyHash key_hash) {
    return new (b->buf) Request(type, key, key_len, val, val_len, key_hash);
  }
  static Request* New(Infiniband::Buffer* b, Type type,
                      const char* key, size_t key_len, KeyHash key_hash) {
    return new (b->buf) Request(type, key, key_len, key_hash);
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
  void Print() const {
    std::cout << "type: " << (type == Type::PUT ? "PUT" : type == Type::GET ? "GET" : "DEL") << std::endl;
    std::cout << "key_len: " << key_len << std::endl;
    std::cout << "val_len: " << val_len << std::endl;
    std::cout << "key_hash: " << key_hash << std::endl;
    std::cout << "key: ";
    for (size_t i = 0; i < key_len; ++i)
      std::cout << data[i];
    std::cout << std::endl;
    std::cout << "val: ";
    for (size_t i = key_len; i < key_len + val_len; ++i)
      std::cout << data[i];
    std::cout << std::endl;
  }

 private:
  Request(Type type, const char* key, size_t key_len,
      const char* val, size_t val_len, KeyHash key_hash)
      : type(type), key_len(key_len), val_len(val_len), key_hash(key_hash) {
    memcpy(data, key, key_len);
    memcpy(data + key_len, val, val_len);
  }
  Request(Type type, const char* key, size_t key_len, KeyHash key_hash)
      : type(type), key_len(key_len), val_len(0), key_hash(key_hash) {
    memcpy(data, key, key_len);
  }
};

} // namespace nvds

#endif // _NVDS_SERVER_H_
