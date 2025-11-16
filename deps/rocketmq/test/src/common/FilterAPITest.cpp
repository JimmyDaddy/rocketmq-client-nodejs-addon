#include <gtest/gtest.h>

#include <memory>

#include "common/FilterAPI.hpp"
#include "common/UtilAll.h"

using rocketmq::FilterAPI;
using rocketmq::MQClientException;
using rocketmq::SubscriptionData;

TEST(FilterAPITest, SubAllWhenExpressionEmpty) {
  std::unique_ptr<SubscriptionData> sub(FilterAPI::buildSubscriptionData("TestTopic", ""));
  EXPECT_EQ(rocketmq::SUB_ALL, sub->sub_string());
  EXPECT_TRUE(sub->tags_set().empty());
  EXPECT_TRUE(sub->code_set().empty());
}

TEST(FilterAPITest, ParsesTrimmedTagsAndCodes) {
  std::unique_ptr<SubscriptionData> sub(
      FilterAPI::buildSubscriptionData("TestTopic", " TagA ||TagB||  TagC  "));

  ASSERT_EQ(3u, sub->tags_set().size());
  EXPECT_EQ("TagA", sub->tags_set()[0]);
  EXPECT_EQ("TagB", sub->tags_set()[1]);
  EXPECT_EQ("TagC", sub->tags_set()[2]);

  ASSERT_EQ(3u, sub->code_set().size());
  EXPECT_EQ(rocketmq::UtilAll::hash_code("TagA"), sub->code_set()[0]);
  EXPECT_EQ(rocketmq::UtilAll::hash_code("TagB"), sub->code_set()[1]);
  EXPECT_EQ(rocketmq::UtilAll::hash_code("TagC"), sub->code_set()[2]);
}

TEST(FilterAPITest, ThrowsWhenSplitProducesNoTokens) {
  EXPECT_THROW(FilterAPI::buildSubscriptionData("Topic", "||"), MQClientException);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "FilterAPITest.*";
  return RUN_ALL_TESTS();
}
