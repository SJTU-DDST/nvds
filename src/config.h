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
  static uint32_t max_nodes() {
    return GetInst()->max_nodes_;
  }
  static uint16_t node_port() {
    return GetInst()->node_port_;
  }

 private:
  Config() {}
  ~Config() {}
  std::string coord_addr_;
  uint16_t coord_port_ {0};
  uint32_t max_nodes_ {0};

  uint16_t node_port_ {0};
};

} // namepsace nvds

#endif // _NVDS_CONFIG_H_
