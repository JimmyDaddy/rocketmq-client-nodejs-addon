#include <gtest/gtest.h>

#include "MQMessageQueue.h"
#include "SendResult.h"
#include "TransactionSendResult.h"

using rocketmq::LocalTransactionState;
using rocketmq::MQMessageQueue;
using rocketmq::SendResult;
using rocketmq::SendStatus;
using rocketmq::TransactionSendResult;

TEST(TransactionSendResultTest, InheritsBaseFieldsAndDefaultsToUnknown) {
  MQMessageQueue queue("Topic", "broker-a", 3);
  SendResult base(SendStatus::SEND_OK, "msg-id", "off-id", queue, 12);
  TransactionSendResult result(base);

  EXPECT_EQ("msg-id", result.msg_id());
  EXPECT_EQ("off-id", result.offset_msg_id());
  EXPECT_EQ(queue.toString(), result.message_queue().toString());
  EXPECT_EQ(12, result.queue_offset());
  EXPECT_EQ(LocalTransactionState::UNKNOWN, result.local_transaction_state());
}

TEST(TransactionSendResultTest, SetterUpdatesLocalTransactionState) {
  SendResult base;
  TransactionSendResult result(base);

  result.set_local_transaction_state(LocalTransactionState::COMMIT_MESSAGE);
  EXPECT_EQ(LocalTransactionState::COMMIT_MESSAGE, result.local_transaction_state());

  result.set_local_transaction_state(LocalTransactionState::ROLLBACK_MESSAGE);
  EXPECT_EQ(LocalTransactionState::ROLLBACK_MESSAGE, result.local_transaction_state());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "TransactionSendResultTest.*";
  return RUN_ALL_TESTS();
}
