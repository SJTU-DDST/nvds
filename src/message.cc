#include "message.h"

#include "index.h"

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

void to_json(nlohmann::json& j, const Infiniband::QueuePairInfo& qpi) {
  j["lid"] = qpi.lid;
  j["qpn"] = qpi.qpn;
  j["psn"] = qpi.psn;
  j["rkey"] = qpi.rkey;
  j["vaddr"] = qpi.vaddr;
}

void from_json(const nlohmann::json& j, Infiniband::QueuePairInfo& qpi) {
  qpi.lid = j["lid"];
  qpi.qpn = j["qpn"];
  qpi.psn = j["psn"];
  qpi.rkey = j["rkey"];
  qpi.vaddr = j["vaddr"];
}

void to_json(nlohmann::json& j, const TabletInfo& ti) {
  j["id"] = ti.id;
  j["server_id"] = ti.server_id;
  j["is_backup"] = ti.is_backup;
  j["qpis"] = ti.qpis;
  if (ti.is_backup) {
    j["master"] = ti.master;
  } else {
    j["backups"] = ti.backups;
  }
}

void from_json(const nlohmann::json& j, TabletInfo& ti) {
  ti.id = j["id"];
  ti.server_id = j["server_id"];
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

void to_json(nlohmann::json& j, const IndexManager& im) {
  j = {
    {"key_tablet_map", im.key_tablet_map_},
    {"tablets", im.tablets_},
    {"servers", im.servers_}
  };
}

void from_json(const nlohmann::json& j, IndexManager& im) {
  im.key_tablet_map_ = j["key_tablet_map"];
  im.tablets_ = j["tablets"];
  im.servers_ = j["servers"];
}

} // namespace nvds
