#include <gtest/gtest.h>

#include <string>

#include "DefaultMQProducer.h"
#include "MQException.h"
#include "MQMessage.h"
#include "UtilAll.h"

using rocketmq::DefaultMQProducer;
using rocketmq::DEFAULT_PRODUCER_GROUP;
using rocketmq::MQClientException;
using rocketmq::MQMessage;

TEST(DefaultMQProducerTest, UsesDefaultGroupWhenGroupNameEmpty) {
  DefaultMQProducer producer("");

  EXPECT_EQ(DEFAULT_PRODUCER_GROUP, producer.group_name());

  producer.shutdown();
}

TEST(DefaultMQProducerTest, SendLatencyFaultEnableRoundTrip) {
  DefaultMQProducer producer("LatencyGroup");
  EXPECT_FALSE(producer.send_latency_fault_enable());

  producer.set_send_latency_fault_enable(true);
  EXPECT_TRUE(producer.send_latency_fault_enable());

  producer.set_send_latency_fault_enable(false);
  EXPECT_FALSE(producer.send_latency_fault_enable());

  producer.shutdown();
}

TEST(DefaultMQProducerTest, SendMessageInTransactionThrows) {
  DefaultMQProducer producer("TxGroup");
  MQMessage message("TxTopic", "payload");

  EXPECT_THROW(producer.sendMessageInTransaction(message, nullptr), MQClientException);

  producer.shutdown();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "DefaultMQProducerTest.*";
  return RUN_ALL_TESTS();
}
