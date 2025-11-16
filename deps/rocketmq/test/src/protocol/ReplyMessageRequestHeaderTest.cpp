#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>

#include "protocol/header/ReplyMessageRequestHeader.hpp"

using rocketmq::ReplyMessageRequestHeader;

namespace {

std::map<std::string, std::string> BuildBaseFields() {
  return {
      {"producerGroup", "groupA"},
      {"topic", "ReplyTopic"},
      {"defaultTopic", "TBW102"},
      {"defaultTopicQueueNums", "8"},
      {"queueId", "3"},
      {"sysFlag", "1"},
      {"bornTimestamp", "1710000000000"},
      {"flag", "4"},
      {"bornHost", "127.0.0.1:10091"},
      {"storeHost", "127.0.0.1:10092"},
      {"storeTimestamp", "1710000001000"},
  };
}

}  // namespace

TEST(ReplyMessageRequestHeaderTest, DecodeIncludesOptionalFieldsWhenPresent) {
  auto fields = BuildBaseFields();
  fields["properties"] = "key=value";
  fields["reconsumeTimes"] = "7";
  fields["unitMode"] = "true";

  std::unique_ptr<ReplyMessageRequestHeader> header(ReplyMessageRequestHeader::Decode(fields));

  ASSERT_NE(nullptr, header);
  EXPECT_EQ("groupA", header->producer_group());
  EXPECT_EQ("ReplyTopic", header->topic());
  EXPECT_EQ("TBW102", header->default_topic());
  EXPECT_EQ(8, header->default_topic_queue_nums());
  EXPECT_EQ(3, header->queue_id());
  EXPECT_EQ(1, header->sys_flag());
  EXPECT_EQ(1710000000000LL, header->born_timestamp());
  EXPECT_EQ(4, header->flag());
  EXPECT_EQ("key=value", header->properties());
  EXPECT_EQ(7, header->reconsume_times());
  EXPECT_TRUE(header->unit_mode());
  EXPECT_EQ("127.0.0.1:10091", header->born_host());
  EXPECT_EQ("127.0.0.1:10092", header->store_host());
  EXPECT_EQ(1710000001000LL, header->store_timestamp());
}

TEST(ReplyMessageRequestHeaderTest, DecodeDefaultsOptionalFieldsWhenMissing) {
  auto fields = BuildBaseFields();

  std::unique_ptr<ReplyMessageRequestHeader> header(ReplyMessageRequestHeader::Decode(fields));

  ASSERT_NE(nullptr, header);
  EXPECT_EQ("", header->properties());
  EXPECT_EQ(0, header->reconsume_times());
  EXPECT_FALSE(header->unit_mode());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ReplyMessageRequestHeaderTest.*";
  return RUN_ALL_TESTS();
}
