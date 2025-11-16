#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "MQMessageConst.h"
#include "MQMessageExt.h"
#include "common/NamespaceUtil.h"
#include "common/UtilAll.h"
#include "consumer/DefaultMQPushConsumerImpl.h"
#include "consumer/DefaultMQPushConsumerConfigImpl.hpp"

using rocketmq::DefaultMQPushConsumerConfigImpl;
using rocketmq::DefaultMQPushConsumerImpl;
using rocketmq::MQMessageConst;
using rocketmq::MQMessageExt;
using rocketmq::MessageExtPtr;

TEST(DefaultMQPushConsumerImplTest, RestoresOriginalTopicForRetryMessages) {
  auto config = std::make_shared<DefaultMQPushConsumerConfigImpl>();
  config->set_group_name("GID_test");

  auto consumer = DefaultMQPushConsumerImpl::create(config);

  auto msg = std::make_shared<MQMessageExt>();
  std::string retry_topic = rocketmq::UtilAll::getRetryTopic(config->group_name());
  msg->set_topic(retry_topic);
  msg->putProperty(MQMessageConst::PROPERTY_RETRY_TOPIC, "UserTopicA");

  std::vector<MessageExtPtr> msgs{msg};
  consumer->resetRetryAndNamespace(msgs);

  EXPECT_EQ("UserTopicA", msg->topic());
}

TEST(DefaultMQPushConsumerImplTest, RemovesNamespaceAfterReset) {
  const std::string kNamespace = "INSTANCE_test";
  const std::string kRawGroup = "GID_order";

  auto config = std::make_shared<DefaultMQPushConsumerConfigImpl>();
  config->set_name_space(kNamespace);
  config->set_group_name(rocketmq::NamespaceUtil::wrapNamespace(kNamespace, kRawGroup));

  auto consumer = DefaultMQPushConsumerImpl::create(config);

  auto retry_msg = std::make_shared<MQMessageExt>();
  std::string namespaced_retry_topic = rocketmq::UtilAll::getRetryTopic(config->group_name());
  retry_msg->set_topic(namespaced_retry_topic);
  retry_msg->putProperty(MQMessageConst::PROPERTY_RETRY_TOPIC,
                         rocketmq::NamespaceUtil::wrapNamespace(kNamespace, "ActualTopic"));

  auto plain_msg = std::make_shared<MQMessageExt>();
  plain_msg->set_topic(rocketmq::NamespaceUtil::wrapNamespace(kNamespace, "PlainTopic"));

  std::vector<MessageExtPtr> msgs{retry_msg, plain_msg};
  consumer->resetRetryAndNamespace(msgs);

  EXPECT_EQ("ActualTopic", retry_msg->topic());
  EXPECT_EQ("PlainTopic", plain_msg->topic());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "DefaultMQPushConsumerImplTest.*";
  return RUN_ALL_TESTS();
}
