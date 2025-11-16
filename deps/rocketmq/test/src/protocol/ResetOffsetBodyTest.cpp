#include <gtest/gtest.h>

#include <memory>

#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>

#include "ByteArray.h"
#include "MQMessageQueue.h"
#include "protocol/body/ResetOffsetBody.hpp"

using rocketmq::ByteArray;
using rocketmq::MQMessageQueue;
using rocketmq::ResetOffsetBody;

namespace {

std::string BuildOffsetTableJson(const std::vector<std::pair<MQMessageQueue, int64_t>>& entries) {
  Json::Value table(Json::objectValue);
  Json::FastWriter writer;

  for (const auto& entry : entries) {
    Json::Value key;
    key["topic"] = entry.first.topic();
    key["brokerName"] = entry.first.broker_name();
    key["queueId"] = entry.first.queue_id();
    table[writer.write(key)] = Json::Int64(entry.second);
  }

  Json::Value root;
  root["offsetTable"] = table;
  return writer.write(root);
}

}  // namespace

TEST(ResetOffsetBodyTest, DecodeBuildsQueueOffsetMap) {
  std::vector<std::pair<MQMessageQueue, int64_t>> entries = {
      {MQMessageQueue("TopicA", "broker-a", 0), 1024},
      {MQMessageQueue("TopicB", "broker-b", 3), 2048},
  };

  std::string json = BuildOffsetTableJson(entries);
  const ByteArray body(const_cast<char*>(json.data()), json.size());

  std::unique_ptr<ResetOffsetBody> decoded(ResetOffsetBody::Decode(body));
  auto& table = decoded->offset_table();

  ASSERT_EQ(entries.size(), table.size());
  for (const auto& entry : entries) {
    auto it = table.find(entry.first);
    ASSERT_NE(table.end(), it);
    EXPECT_EQ(entry.second, it->second);
  }
}

TEST(ResetOffsetBodyTest, DecodeHandlesEmptyOffsetTable) {
  Json::Value root;
  root["offsetTable"] = Json::Value(Json::objectValue);
  Json::FastWriter writer;
  std::string json = writer.write(root);
  const ByteArray body(const_cast<char*>(json.data()), json.size());

  std::unique_ptr<ResetOffsetBody> decoded(ResetOffsetBody::Decode(body));
  EXPECT_TRUE(decoded->offset_table().empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ResetOffsetBodyTest.*";
  return RUN_ALL_TESTS();
}
