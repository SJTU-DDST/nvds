#ifndef _NVDS_CONFIG_H_
#define _NVDS_CONFIG_H_

#include "common.h"

namespace nvds {

class Config {
 public:
  static Config* GetInst() {
    static auto inst = new Config();
    return inst;
  }

  void Load(const char* cfg_file);
  static const std::string& coord_addr() {
    return GetInst()->coord_addr_;
  }
  static uint16_t coord_port() {
    return GetInst()->coord_port_;
  }
  static uint32_t num_servers() {
    return GetInst()->num_servers_;
  }
  static uint16_t server_port() {
    return GetInst()->server_port_;
  }

 private:
  Config() {}
  ~Config() {}
  std::string coord_addr_;
  uint16_t coord_port_ {0};
  uint32_t num_servers_ {0};

  uint16_t server_port_ {0};
};

} // namepsace nvds

#endif // _NVDS_CONFIG_H_
