#include <gtest/gtest.h>

#include "consumer/FindBrokerResult.hpp"

using rocketmq::FindBrokerResult;

TEST(FindBrokerResultTest, StoresBrokerAddressAndSlaveFlag) {
  FindBrokerResult result("127.0.0.1:10911", false);

  EXPECT_EQ("127.0.0.1:10911", result.broker_addr());
  EXPECT_FALSE(result.slave());

  result.set_borker_addr("192.168.1.2:10912");
  result.set_slave(true);

  EXPECT_EQ("192.168.1.2:10912", result.broker_addr());
  EXPECT_TRUE(result.slave());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "FindBrokerResultTest.*";
  return RUN_ALL_TESTS();
}
