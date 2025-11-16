#include <gtest/gtest.h>

#include <map>
#include <string>

#include "MQMessage.h"
#include "MQMessageConst.h"
#include "message/MessageAccessor.hpp"

using rocketmq::MQMessage;
using rocketmq::MQMessageConst;
using rocketmq::MessageAccessor;

TEST(MessageAccessorTest, SetPropertiesOverridesExistingValues) {
  MQMessage message("TopicA", "body");
  message.putProperty("legacy", "old");

  std::map<std::string, std::string> props{{"key1", "value1"}, {"key2", "value2"}};
  MessageAccessor::setProperties(message, std::move(props));

  EXPECT_EQ("value1", message.getProperty("key1"));
  EXPECT_EQ("value2", message.getProperty("key2"));
  EXPECT_EQ(2u, message.properties().size());
}

TEST(MessageAccessorTest, PutAndClearSingleProperty) {
  MQMessage message("TopicB", "body");
  MessageAccessor::putProperty(message, "temp", "123");
  EXPECT_EQ("123", message.getProperty("temp"));

  MessageAccessor::clearProperty(message, "temp");
  EXPECT_TRUE(message.properties().find("temp") == message.properties().end());
}

TEST(MessageAccessorTest, ReconsumeMetadataAccessors) {
  MQMessage message("TopicC", "body");
  MessageAccessor::putProperty(message, MQMessageConst::PROPERTY_RECONSUME_TIME, "5");
  MessageAccessor::putProperty(message, MQMessageConst::PROPERTY_MAX_RECONSUME_TIMES, "16");

  EXPECT_EQ("5", MessageAccessor::getReconsumeTime(message));
  EXPECT_EQ("16", MessageAccessor::getMaxReconsumeTimes(message));
}

TEST(MessageAccessorTest, SetsConsumeStartTimestampProperty) {
  MQMessage message("TopicD", "body");
  MessageAccessor::setConsumeStartTimeStamp(message, "1700000000");
  EXPECT_EQ("1700000000", message.getProperty(MQMessageConst::PROPERTY_CONSUME_START_TIMESTAMP));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MessageAccessorTest.*";
  return RUN_ALL_TESTS();
}
