#include <gtest/gtest.h>

#include "MQException.h"
#include "MQMessage.h"
#include "MQMessageConst.h"
#include "MessageUtil.h"
#include "common/UtilAll.h"

using rocketmq::MQClientException;
using rocketmq::MQMessage;
using rocketmq::MQMessageConst;
using rocketmq::MessageUtil;
using rocketmq::REPLY_MESSAGE_FLAG;
using rocketmq::UtilAll;

TEST(MessageUtilTest, CreateReplyMessageCopiesMetadata) {
  MQMessage request("ReqTopic", "tag", "body");
  request.putProperty(MQMessageConst::PROPERTY_CLUSTER, "TestCluster");
  request.putProperty(MQMessageConst::PROPERTY_CORRELATION_ID, "corr-123");
  request.putProperty(MQMessageConst::PROPERTY_MESSAGE_REPLY_TO_CLIENT, "client-xyz");
  request.putProperty(MQMessageConst::PROPERTY_MESSAGE_TTL, "3000");

  MQMessage reply = MessageUtil::createReplyMessage(request, "reply-body");

  EXPECT_EQ(UtilAll::getReplyTopic("TestCluster"), reply.topic());
  EXPECT_EQ(REPLY_MESSAGE_FLAG, reply.getProperty(MQMessageConst::PROPERTY_MESSAGE_TYPE));
  EXPECT_EQ("corr-123", reply.getProperty(MQMessageConst::PROPERTY_CORRELATION_ID));
  EXPECT_EQ("client-xyz", reply.getProperty(MQMessageConst::PROPERTY_MESSAGE_REPLY_TO_CLIENT));
  EXPECT_EQ("3000", reply.getProperty(MQMessageConst::PROPERTY_MESSAGE_TTL));
  EXPECT_EQ("reply-body", reply.body());
}

TEST(MessageUtilTest, CreateReplyMessageThrowsWhenClusterMissing) {
  MQMessage request("ReqTopic", "body");
  EXPECT_THROW({ MessageUtil::createReplyMessage(request, "reply"); }, MQClientException);
}

TEST(MessageUtilTest, GetReplyToClientReadsProperty) {
  MQMessage request("ReqTopic", "body");
  request.putProperty(MQMessageConst::PROPERTY_MESSAGE_REPLY_TO_CLIENT, "client-abc");
  EXPECT_EQ("client-abc", MessageUtil::getReplyToClient(request));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MessageUtilTest.*";
  return RUN_ALL_TESTS();
}
