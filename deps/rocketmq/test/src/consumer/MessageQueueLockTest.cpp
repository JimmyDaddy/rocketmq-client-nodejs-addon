#include <gtest/gtest.h>

#include <memory>

#include "MQMessageQueue.h"
#include "consumer/MessageQueueLock.hpp"

using rocketmq::MessageQueueLock;
using rocketmq::MQMessageQueue;

TEST(MessageQueueLockTest, ReturnsSameMutexForSameQueue) {
  MessageQueueLock lock_table;
  MQMessageQueue queue("TestTopic", "brokerA", 0);

  auto first = lock_table.fetchLockObject(queue);
  auto second = lock_table.fetchLockObject(queue);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first.get(), second.get());
}

TEST(MessageQueueLockTest, DifferentQueuesGetDifferentMutexes) {
  MessageQueueLock lock_table;
  MQMessageQueue queue0("TestTopic", "brokerA", 0);
  MQMessageQueue queue1("TestTopic", "brokerA", 1);

  auto first = lock_table.fetchLockObject(queue0);
  auto second = lock_table.fetchLockObject(queue1);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first.get(), second.get());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MessageQueueLockTest.*";
  return RUN_ALL_TESTS();
}
