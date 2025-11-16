#include <gtest/gtest.h>

#include "ExpressionType.h"

using rocketmq::ExpressionType;

TEST(ExpressionTypeTest, TreatsEmptyAndTagAsTagType) {
  EXPECT_TRUE(ExpressionType::isTagType(""));
  EXPECT_TRUE(ExpressionType::isTagType(ExpressionType::TAG));
}

TEST(ExpressionTypeTest, RejectsNonTagExpressions) {
  EXPECT_FALSE(ExpressionType::isTagType(ExpressionType::SQL92));
  EXPECT_FALSE(ExpressionType::isTagType("CUSTOM"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ExpressionTypeTest.*";
  return RUN_ALL_TESTS();
}
