#include <gtest/gtest.h>

#include "SubscriptionGroupConfig.h"

using rocketmq::SubscriptionGroupConfig;

TEST(SubscriptionGroupConfigTest, InitializesDefaultValues) {
  SubscriptionGroupConfig config("groupA");

  EXPECT_EQ("groupA", config.groupName);
  EXPECT_TRUE(config.consumeEnable);
  EXPECT_TRUE(config.consumeFromMinEnable);
  EXPECT_TRUE(config.consumeBroadcastEnable);
  EXPECT_EQ(1, config.retryQueueNums);
  EXPECT_EQ(5, config.retryMaxTimes);
  EXPECT_EQ(rocketmq::MASTER_ID, config.brokerId);
  EXPECT_EQ(1, config.whichBrokerWhenConsumeSlowly);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "SubscriptionGroupConfigTest.*";
  return RUN_ALL_TESTS();
}
