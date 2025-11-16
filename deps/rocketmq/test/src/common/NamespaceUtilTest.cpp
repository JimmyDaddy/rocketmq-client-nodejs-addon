#include <gtest/gtest.h>

#include <string>

#include "common/NamespaceUtil.h"
#include "common/UtilAll.h"

using rocketmq::NamespaceUtil;

TEST(NamespaceUtilTest, WrapsAndUnwrapsStandardTopic) {
  const std::string kNamespace = "INSTANCE_ns";
  const std::string kTopic = "UserTopic";

  std::string namespaced = NamespaceUtil::wrapNamespace(kNamespace, kTopic);
  EXPECT_EQ(kNamespace + "%" + kTopic, namespaced);

  EXPECT_EQ(kTopic, NamespaceUtil::withoutNamespace(namespaced));
  EXPECT_EQ(kTopic, NamespaceUtil::withoutNamespace(namespaced, kNamespace));

  // Different namespace should leave string intact.
  EXPECT_EQ(namespaced, NamespaceUtil::withoutNamespace(namespaced, "OTHER"));
}

TEST(NamespaceUtilTest, HandlesRetryTopicsWhenWrapping) {
  const std::string kNamespace = "INSTANCE_retry";
  const std::string kGroup = "GID_group";
  std::string retryTopic = rocketmq::UtilAll::getRetryTopic(kGroup);

  std::string namespacedRetry = NamespaceUtil::wrapNamespace(kNamespace, retryTopic);
  std::string expected = rocketmq::UtilAll::getRetryTopic(kNamespace + "%" + kGroup);
  EXPECT_EQ(expected, namespacedRetry);

  // withoutNamespace should revert to the original retry topic (without namespace).
  EXPECT_EQ(retryTopic, NamespaceUtil::withoutNamespace(namespacedRetry));
  EXPECT_EQ(retryTopic, NamespaceUtil::withoutNamespace(namespacedRetry, kNamespace));
}

TEST(NamespaceUtilTest, HandlesDLQTopicsWhenWrapping) {
  const std::string kNamespace = "INSTANCE_dlq";
  const std::string kGroup = "GID_failed";
  std::string dlqTopic = rocketmq::UtilAll::getDLQTopic(kGroup);

  std::string namespacedDlq = NamespaceUtil::wrapNamespace(kNamespace, dlqTopic);
  std::string expected = rocketmq::UtilAll::getDLQTopic(kNamespace + "%" + kGroup);
  EXPECT_EQ(expected, namespacedDlq);

  EXPECT_EQ(dlqTopic, NamespaceUtil::withoutNamespace(namespacedDlq));
  EXPECT_EQ(dlqTopic, NamespaceUtil::withoutNamespace(namespacedDlq, kNamespace));
}

TEST(NamespaceUtilTest, WithoutRetryAndDLQStripsPrefixes) {
  const std::string kGroup = "GID_strip";
  EXPECT_EQ(kGroup, NamespaceUtil::withoutRetryAndDLQ(rocketmq::UtilAll::getRetryTopic(kGroup)));
  EXPECT_EQ(kGroup, NamespaceUtil::withoutRetryAndDLQ(rocketmq::UtilAll::getDLQTopic(kGroup)));
  EXPECT_EQ("PlainTopic", NamespaceUtil::withoutRetryAndDLQ("PlainTopic"));
}

TEST(NamespaceUtilTest, DetectsEndPointUrls) {
  EXPECT_TRUE(NamespaceUtil::isEndPointURL("http://mq.example.com:8080"));
  EXPECT_FALSE(NamespaceUtil::isEndPointURL("mq.example.com:9876"));

  EXPECT_EQ("mq.example.com:8080", NamespaceUtil::formatNameServerURL("http://mq.example.com:8080"));
  EXPECT_EQ("10.0.0.1:9876", NamespaceUtil::formatNameServerURL("10.0.0.1:9876"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "NamespaceUtilTest.*";
  return RUN_ALL_TESTS();
}
