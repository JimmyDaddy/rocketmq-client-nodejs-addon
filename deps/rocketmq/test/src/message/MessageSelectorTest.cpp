#include <gtest/gtest.h>

#include <utility>

#include "ExpressionType.h"
#include "MessageSelector.h"

using rocketmq::ExpressionType;
using rocketmq::MessageSelector;

TEST(MessageSelectorTest, BySqlFactorySetsTypeAndExpression) {
  auto selector = MessageSelector::bySql("age > 18");
  EXPECT_EQ(ExpressionType::SQL92, selector.type());
  EXPECT_EQ("age > 18", selector.expression());
}

TEST(MessageSelectorTest, ByTagFactoryUsesTagType) {
  auto selector = MessageSelector::byTag("TagA || TagB");
  EXPECT_EQ(ExpressionType::TAG, selector.type());
  EXPECT_EQ("TagA || TagB", selector.expression());
}

TEST(MessageSelectorTest, CopyAndMoveSemantics) {
  auto original = MessageSelector::bySql("x = 1");
  MessageSelector copy(original);
  EXPECT_EQ(original.type(), copy.type());
  EXPECT_EQ(original.expression(), copy.expression());

  MessageSelector moved(std::move(original));
  EXPECT_EQ(copy.type(), moved.type());
  EXPECT_EQ(copy.expression(), moved.expression());

  MessageSelector assigned = MessageSelector::byTag("TagC");
  assigned = copy;
  EXPECT_EQ(copy.type(), assigned.type());
  EXPECT_EQ(copy.expression(), assigned.expression());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MessageSelectorTest.*";
  return RUN_ALL_TESTS();
}
