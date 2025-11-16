#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "MQMessageExt.h"
#include "consumer/PullResult.h"

using rocketmq::MQMessageExt;
using rocketmq::MessageExtPtr;
using rocketmq::PullResult;
using rocketmq::PullStatus;

namespace {

MessageExtPtr MakeMessage(const std::string& topic, const std::string& tags) {
  auto msg = std::make_shared<MQMessageExt>();
  msg->set_topic(topic);
  msg->set_tags(tags);
  return msg;
}

}  // namespace

TEST(PullResultTest, DefaultConstructorInitializesWithNoMatch) {
  PullResult result;
  EXPECT_EQ(PullStatus::NO_MATCHED_MSG, result.pull_status());
  EXPECT_EQ(0, result.next_begin_offset());
  EXPECT_EQ(0, result.min_offset());
  EXPECT_EQ(0, result.max_offset());
  EXPECT_TRUE(result.msg_found_list().empty());
}

TEST(PullResultTest, StoresMessageListByCopy) {
  std::vector<MessageExtPtr> messages = {MakeMessage("A", "t1"), MakeMessage("A", "t2")};
  PullResult result(PullStatus::FOUND, 11, 5, 20, messages);

  EXPECT_EQ(PullStatus::FOUND, result.pull_status());
  EXPECT_EQ(11, result.next_begin_offset());
  EXPECT_EQ(5, result.min_offset());
  EXPECT_EQ(20, result.max_offset());
  ASSERT_EQ(messages.size(), result.msg_found_list().size());
  EXPECT_EQ(messages[0], result.msg_found_list()[0]);
  EXPECT_EQ(messages[1], result.msg_found_list()[1]);
}

TEST(PullResultTest, AcceptsMovedMessageList) {
  std::vector<MessageExtPtr> messages;
  messages.push_back(MakeMessage("B", "x"));
  PullResult result(PullStatus::FOUND, 7, 3, 15, std::move(messages));

  ASSERT_EQ(1u, result.msg_found_list().size());
  EXPECT_EQ("B", result.msg_found_list()[0]->topic());
  EXPECT_EQ("x", result.msg_found_list()[0]->tags());
}

TEST(PullResultTest, ToStringContainsStatusAndOffsets) {
  PullResult result(PullStatus::NO_NEW_MSG, 42, 10, 100);
  auto text = result.toString();

  EXPECT_NE(std::string::npos, text.find("NO_NEW_MSG"));
  EXPECT_NE(std::string::npos, text.find("nextBeginOffset=42"));
  EXPECT_NE(std::string::npos, text.find("minOffset=10"));
  EXPECT_NE(std::string::npos, text.find("maxOffset=100"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "PullResultTest.*";
  return RUN_ALL_TESTS();
}
