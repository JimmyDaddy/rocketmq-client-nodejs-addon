#include <gtest/gtest.h>

#include <vector>

#include "MQMessageConst.h"
#include "MessageImpl.h"

using rocketmq::MessageImpl;
using rocketmq::MQMessageConst;

TEST(MessageImplTest, ConstructorPopulatesTagsKeysAndBody) {
  MessageImpl message("TopicA", "TagA", "KeyA", 3, "Body", false);

  EXPECT_EQ("TopicA", message.topic());
  EXPECT_EQ("TagA", message.tags());
  EXPECT_EQ("KeyA", message.keys());
  EXPECT_EQ(3, message.flag());
  EXPECT_EQ("Body", message.body());
  EXPECT_FALSE(message.wait_store_msg_ok());

  const auto text = message.toString();
  EXPECT_NE(std::string::npos, text.find("topic=TopicA"));
  EXPECT_NE(std::string::npos, text.find("tag=TagA"));
}

TEST(MessageImplTest, ConcatenatesVectorKeysWithSeparator) {
  MessageImpl message;
  message.set_keys(std::vector<std::string>{"id1", "id2", "id3"});

  const std::string expected = std::string("id1") + MQMessageConst::KEY_SEPARATOR + "id2" +
                               MQMessageConst::KEY_SEPARATOR + "id3";
  EXPECT_EQ(expected, message.keys());

  message.set_keys(std::vector<std::string>{});
  EXPECT_EQ(expected, message.keys()) << "Empty vector should leave existing keys untouched";
}

TEST(MessageImplTest, DelayLevelAndWaitStoreMsgOkParsing) {
  MessageImpl message;
  EXPECT_TRUE(message.wait_store_msg_ok()) << "Absent property defaults to true";
  EXPECT_EQ(0, message.delay_time_level());

  message.set_delay_time_level(7);
  EXPECT_EQ(7, message.delay_time_level());

  message.set_wait_store_msg_ok(false);
  EXPECT_FALSE(message.wait_store_msg_ok());

  message.set_wait_store_msg_ok(true);
  EXPECT_TRUE(message.wait_store_msg_ok());
}

TEST(MessageImplTest, PropertyAccessorsModifyBackingMap) {
  MessageImpl message;
  message.putProperty("custom", "value1");
  EXPECT_EQ("value1", message.getProperty("custom"));

  message.clearProperty("custom");
  EXPECT_TRUE(message.getProperty("custom").empty());

  std::map<std::string, std::string> props = {{"foo", "bar"}, {MQMessageConst::PROPERTY_TAGS, "TagX"}};
  message.set_properties(props);
  EXPECT_EQ("bar", message.getProperty("foo"));
  EXPECT_EQ("TagX", message.tags());

  message.set_properties({{"alpha", "beta"}});
  EXPECT_TRUE(message.getProperty("foo").empty());
  EXPECT_EQ("beta", message.getProperty("alpha"));
}

TEST(MessageImplTest, TopicSetFromCharPointer) {
  MessageImpl message;
  const char topic[] = {'T', 'p', 'c', 'X'};
  message.set_topic(topic, sizeof(topic));
  EXPECT_EQ(std::string(topic, sizeof(topic)), message.topic());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MessageImplTest.*";
  return RUN_ALL_TESTS();
}
