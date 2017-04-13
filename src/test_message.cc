#include "json.hpp"
#include "message.h"
#include <gtest/gtest.h>

using namespace std;
using namespace nvds;
using namespace nlohmann;

TEST (MessageTest, Backup) {
  string r = R"([123, 456])";
  Backup b = json::parse(r);
  EXPECT_EQ(b.server_id, 123);
  EXPECT_EQ(b.tablet_id, 456);

  json j = Backup {111, 222};
  EXPECT_EQ(j[0], 111);
  EXPECT_EQ(j[1], 222);
}

TEST (MessageTest, TabletInfo) {
  string r = R"({
    "id": 3,
    "is_backup": true,
    "master": [123, 456]
  })";
  TabletInfo t = json::parse(r);
  EXPECT_EQ(t.id, 3);
  EXPECT_EQ(t.is_backup, true);
  EXPECT_EQ(t.master.server_id, 123);
  EXPECT_EQ(t.master.tablet_id, 456);
  
  t = TabletInfo { 3, true};
  t.master = {123, 456};
  json j = t;
  EXPECT_EQ(j["id"], 3);
  EXPECT_EQ(j["is_backup"], true);
  EXPECT_EQ(j["master"][0], 123);
  EXPECT_EQ(j["master"][1], 456);
  
  
  r = R"({
    "id": 3,
    "is_backup": false,
    "backups": [[123, 456], [789, 101112]]
  })";
  t = json::parse(r);
  EXPECT_EQ(t.id, 3);
  EXPECT_EQ(t.is_backup, false);
  EXPECT_EQ(t.backups[0].server_id, 123);
  EXPECT_EQ(t.backups[0].tablet_id, 456);
  EXPECT_EQ(t.backups[1].server_id, 789);
  EXPECT_EQ(t.backups[1].tablet_id, 101112);

  //j = TabletInfo { 3, false};
  t = {3, false};
  t.backups[0] = {123, 456};
  t.backups[1] = {789, 101112};
  j = t;
  EXPECT_EQ(j["id"], 3);
  EXPECT_EQ(j["is_backup"], false);
  EXPECT_EQ(j["backups"][0][0], 123);
  EXPECT_EQ(j["backups"][0][1], 456);
  EXPECT_EQ(j["backups"][1][0], 789);
  EXPECT_EQ(j["backups"][1][1], 101112);
}

TEST (MessageTest, InfinibandAddress) {
  string r = R"({
    "ib_port": 1,
    "lid": 2,
    "qpn": 3
  })";
  Infiniband::Address addr = json::parse(r);
  EXPECT_EQ(addr.ib_port, 1);
  EXPECT_EQ(addr.lid, 2);
  EXPECT_EQ(addr.qpn, 3);

  json j = Infiniband::Address {1, 2, 3};
  EXPECT_EQ(j["ib_port"], 1);
  EXPECT_EQ(j["lid"], 2);
  EXPECT_EQ(j["qpn"], 3);
}

TEST (MessageTest, ServerInfo) {
  string r = R"({
    "id": 1,
    "active": true,
    "addr": "192.168.1.12",
    "ib_addr": {
      "ib_port": 11,
      "lid": 12,
      "qpn": 13
    },
    "tablets": [
      {
        "id": 0,
        "is_backup": true,
        "master": [11, 13]
      }
    ]
  })";
  ServerInfo s = json::parse(r);
  EXPECT_EQ(s.id, 1);
  EXPECT_EQ(s.active, true);
  EXPECT_EQ(s.addr, "192.168.1.12");
  EXPECT_EQ(s.ib_addr, Infiniband::Address({11, 12, 13}));
  EXPECT_EQ(s.tablets[0].id, 0);
  EXPECT_EQ(s.tablets[0].is_backup, true);
  EXPECT_EQ(s.tablets[0].master, Backup({11, 13}));

  json j = s;
  EXPECT_EQ(j["id"], 1);
  EXPECT_EQ(j["active"], true);
  EXPECT_EQ(j["addr"], "192.168.1.12");
  EXPECT_EQ(j["ib_addr"], Infiniband::Address({11, 12, 13}));
  EXPECT_EQ(j["tablets"][0]["id"], 0);
  EXPECT_EQ(j["tablets"][0]["is_backup"], true);
  EXPECT_EQ(j["tablets"][0]["master"][0], 11);
  EXPECT_EQ(j["tablets"][0]["master"][1], 13);
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
  return 0;
}
