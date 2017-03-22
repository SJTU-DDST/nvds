#include "config.h"
#include "json.hpp"

#include <fstream>

namespace nvds {

using json = nlohmann::json;

void Config::Load(const char* cfg_file) {
  json cfg;
  
  try {
    cfg = json::parse(std::ifstream(cfg_file));
  } catch (std::invalid_argument& inv_arg) {
    NVDS_ERR("cannot open config file '%s'", cfg_file);
    throw inv_arg;
  }

  auto& cfg_coord = cfg["coordinator"];
  coord_addr_ = cfg_coord["address"];
  coord_port_ = cfg_coord["tcp_port"];
  max_nodes_ = cfg_coord["max_nodes"];

  auto& cfg_node = cfg["node"];
  node_port_ = cfg_node["tcp_port"];
}

} // namespace nvds
