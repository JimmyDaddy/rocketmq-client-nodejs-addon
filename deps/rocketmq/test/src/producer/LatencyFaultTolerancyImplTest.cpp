#include <gtest/gtest.h>

#include <string>

#include "producer/LatencyFaultTolerancyImpl.h"

using rocketmq::LatencyFaultTolerancyImpl;

class LatencyFaultTolerancyImplTest : public ::testing::Test {
 protected:
  LatencyFaultTolerancyImpl faultTolerance_;
};

TEST_F(LatencyFaultTolerancyImplTest, UnknownBrokerIsAvailableByDefault) {
  EXPECT_TRUE(faultTolerance_.isAvailable("unknown"));

  faultTolerance_.updateFaultItem("brokerA", 100, 1000);
  EXPECT_FALSE(faultTolerance_.isAvailable("brokerA"));

  faultTolerance_.remove("brokerA");
  EXPECT_TRUE(faultTolerance_.isAvailable("brokerA"));
}

TEST_F(LatencyFaultTolerancyImplTest, PickOnePrefersAvailableBroker) {
  faultTolerance_.updateFaultItem("brokerGood", 10, 0);
  faultTolerance_.updateFaultItem("brokerBad", 10, 1000);

  std::string picked = faultTolerance_.pickOneAtLeast();
  EXPECT_EQ("brokerGood", picked);
}

TEST_F(LatencyFaultTolerancyImplTest, AvailableBrokersOrderedByLatency) {
  faultTolerance_.updateFaultItem("slow", 1000, -1);
  faultTolerance_.updateFaultItem("fast", 100, -1);

  std::string pickedFirst = faultTolerance_.pickOneAtLeast();
  EXPECT_EQ("fast", pickedFirst);

  // Remove the first pick and ensure the other broker is returned next.
  faultTolerance_.remove(pickedFirst);
  std::string pickedSecond = faultTolerance_.pickOneAtLeast();
  EXPECT_EQ("slow", pickedSecond);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "LatencyFaultTolerancyImplTest.*";
  return RUN_ALL_TESTS();
}
