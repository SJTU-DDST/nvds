#include "message.h"

namespace nvds {

void to_json(nlohmann::json& j, const Infiniband::Address& ia) {
  j = {
    {"ib_port", ia.ib_port},
    {"lid", ia.lid},
    {"qpn", ia.qpn}
  };
}

void from_json(const nlohmann::json& j, Infiniband::Address& ia) {
  ia = {
    j["ib_port"],
    j["lid"],
    j["qpn"],
  };
}

void to_json(nlohmann::json& j, const Backup& b) {
  j = nlohmann::json::array({b.server_id, b.tablet_id});
}

void from_json(const nlohmann::json& j, Backup& b) {
  b = {j[0], j[1]};
}

void to_json(nlohmann::json& j, const TabletInfo& ti) {
  j["id"] = ti.id;
  j["is_backup"] = ti.is_backup;
  if (ti.is_backup) {
    j["master"] = ti.master;
  } else {
    j["backups"] = ti.backups;
  }
}

void from_json(const nlohmann::json& j, TabletInfo& ti) {
  ti.id = j["id"];
  ti.is_backup = j["is_backup"];
  if (ti.is_backup) {
    ti.master = j["master"];
  } else {
    ti.backups = j["backups"];
  }
}

void to_json(nlohmann::json& j, const ServerInfo& si) {
  j = {
    {"id", si.id},
    {"active", si.active},
    {"addr", si.addr},
    {"ib_addr", si.ib_addr},
    {"tablets", si.tablets}
  };
}

void from_json(const nlohmann::json& j, ServerInfo& si) {
  si = {
    j["id"],
    j["active"],
    j["addr"],
    j["ib_addr"],
    j["tablets"]
  };
}

} // namespace nvds
