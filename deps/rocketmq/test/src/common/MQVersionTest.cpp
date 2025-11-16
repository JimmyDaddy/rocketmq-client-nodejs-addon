#include <gtest/gtest.h>

#include <string>

#include "MQVersion.h"

using rocketmq::MQVersion;

TEST(MQVersionTest, ReturnsExactDescriptorForKnownVersion) {
  EXPECT_STREQ("V4_6_0", MQVersion::GetVersionDesc(MQVersion::V4_6_0));
  EXPECT_STREQ(MQVersion::CURRENT_LANGUAGE.c_str(), "CPP");
  EXPECT_EQ(MQVersion::CURRENT_VERSION, MQVersion::V4_6_0);
}

TEST(MQVersionTest, ClampsBelowMinimumToSnapshot) {
  EXPECT_STREQ("V3_0_0_SNAPSHOT", MQVersion::GetVersionDesc(-100));
}

TEST(MQVersionTest, ClampsAboveHighestToHigherVersionMarker) {
  EXPECT_STREQ("HIGHER_VERSION", MQVersion::GetVersionDesc(MQVersion::HIGHER_VERSION));
  EXPECT_STREQ("HIGHER_VERSION", MQVersion::GetVersionDesc(MQVersion::HIGHER_VERSION + 42));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQVersionTest.*";
  return RUN_ALL_TESTS();
}
