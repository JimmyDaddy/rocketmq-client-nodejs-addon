#include <gtest/gtest.h>

#include <json/json.h>

#include "protocol/heartbeat/SubscriptionData.hpp"

using rocketmq::SubscriptionData;

TEST(SubscriptionDataTest, ComparisonOrdersByTopicThenSubString) {
  SubscriptionData first("TopicA", "tagA");
  SubscriptionData second("TopicB", "tagB");
  SubscriptionData third("TopicA", "tagB");

  EXPECT_TRUE(first < second);
  EXPECT_TRUE(first < third);
  EXPECT_FALSE(second < first);
}

TEST(SubscriptionDataTest, ContainsTagReflectsTagSet) {
  SubscriptionData data("Topic", "* ");
  data.tags_set().push_back("TagA");
  data.tags_set().push_back("TagB");

  EXPECT_TRUE(data.containsTag("TagA"));
  EXPECT_FALSE(data.containsTag("Missing"));
}

TEST(SubscriptionDataTest, ToJsonIncludesTagsCodesAndVersions) {
  SubscriptionData data("Topic", "A || B");
  data.tags_set().push_back("A");
  data.tags_set().push_back("B");
  data.code_set().push_back(123);
  data.code_set().push_back(456);

  Json::Value json = data.toJson();
  EXPECT_EQ("Topic", json["topic"].asString());
  EXPECT_EQ("A || B", json["subString"].asString());
  ASSERT_EQ(2u, json["tagsSet"].size());
  EXPECT_EQ("A", json["tagsSet"][0].asString());
  EXPECT_EQ("B", json["tagsSet"][1].asString());
  ASSERT_EQ(2u, json["codeSet"].size());
  EXPECT_EQ(123, json["codeSet"][0].asInt());
  EXPECT_EQ(456, json["codeSet"][1].asInt());
  EXPECT_FALSE(json["subVersion"].asString().empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "SubscriptionDataTest.*";
  return RUN_ALL_TESTS();
}
