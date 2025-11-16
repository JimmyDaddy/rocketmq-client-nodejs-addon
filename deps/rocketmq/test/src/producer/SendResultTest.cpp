#include <gtest/gtest.h>

#include "MQMessageQueue.h"
#include "SendResult.h"

using rocketmq::MQMessageQueue;
using rocketmq::SendResult;
using rocketmq::SendStatus;

TEST(SendResultTest, DefaultConstructorInitializesFields) {
  SendResult result;
  EXPECT_EQ(SendStatus::SEND_OK, result.send_status());
  EXPECT_EQ(0, result.queue_offset());
  EXPECT_TRUE(result.msg_id().empty());
  EXPECT_TRUE(result.offset_msg_id().empty());
  EXPECT_TRUE(result.transaction_id().empty());
}

TEST(SendResultTest, SettersUpdateFields) {
  MQMessageQueue queue("Topic", "broker-a", 3);
  SendResult result(SendStatus::SEND_FLUSH_DISK_TIMEOUT, "mid", "off", queue, 12);

  result.send_status(SendStatus::SEND_SLAVE_NOT_AVAILABLE);
  result.msg_id("id-2");
  std::string offset = "offset-2";
  result.offset_msg_id(offset);
  MQMessageQueue new_queue("Topic", "broker-b", 7);
  result.message_queue(new_queue);
  result.set_queue_offset(99);
  result.set_transaction_id("txn-123");

  EXPECT_EQ(SendStatus::SEND_SLAVE_NOT_AVAILABLE, result.send_status());
  EXPECT_EQ("id-2", result.msg_id());
  EXPECT_EQ("offset-2", result.offset_msg_id());
  EXPECT_EQ(new_queue.toString(), result.message_queue().toString());
  EXPECT_EQ(99, result.queue_offset());
  EXPECT_EQ("txn-123", result.transaction_id());
}

TEST(SendResultTest, ToStringContainsAllFields) {
  MQMessageQueue queue("TopicA", "broker-x", 5);
  SendResult result(SendStatus::SEND_OK, "msg", "off", queue, 88);
  result.set_transaction_id("txid");

  std::string text = result.toString();
  EXPECT_NE(std::string::npos,
            text.find(std::string("sendStatus:") + std::to_string(static_cast<int>(SendStatus::SEND_OK))));
  EXPECT_NE(std::string::npos, text.find("msgId:msg"));
  EXPECT_NE(std::string::npos, text.find("offsetMsgId:off"));
  EXPECT_NE(std::string::npos, text.find("queueOffset:88"));
  EXPECT_NE(std::string::npos, text.find("transactionId:txid"));
  EXPECT_NE(std::string::npos, text.find(queue.toString()));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "SendResultTest.*";
  return RUN_ALL_TESTS();
}
