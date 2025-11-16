#include <gtest/gtest.h>

#include "MQMessageQueue.h"
#include "RemoteBrokerOffsetStore.h"

using rocketmq::MQMessageQueue;
using rocketmq::ReadOffsetType;
using rocketmq::RemoteBrokerOffsetStore;

namespace {

MQMessageQueue MakeQueue(int queue_id) {
  return MQMessageQueue("TopicTest", "broker-a", queue_id);
}

}  // namespace

TEST(RemoteBrokerOffsetStoreTest, UpdateOffsetRespectsIncreaseOnlyFlag) {
  RemoteBrokerOffsetStore store(nullptr, "group");
  MQMessageQueue mq = MakeQueue(0);

  store.updateOffset(mq, 100, true);
  EXPECT_EQ(100, store.readOffset(mq, ReadOffsetType::READ_FROM_MEMORY));

  store.updateOffset(mq, 90, true);
  EXPECT_EQ(100, store.readOffset(mq, ReadOffsetType::READ_FROM_MEMORY));

  store.updateOffset(mq, 90, false);
  EXPECT_EQ(90, store.readOffset(mq, ReadOffsetType::READ_FROM_MEMORY));

  store.updateOffset(mq, 110, true);
  EXPECT_EQ(110, store.readOffset(mq, ReadOffsetType::READ_FROM_MEMORY));
}

TEST(RemoteBrokerOffsetStoreTest, RemoveOffsetClearsOnlyTargetQueue) {
  RemoteBrokerOffsetStore store(nullptr, "group");
  MQMessageQueue mq1 = MakeQueue(1);
  MQMessageQueue mq2 = MakeQueue(2);

  store.updateOffset(mq1, 12, false);
  store.updateOffset(mq2, 34, false);

  store.removeOffset(mq1);

  EXPECT_EQ(-1, store.readOffset(mq1, ReadOffsetType::READ_FROM_MEMORY));
  EXPECT_EQ(34, store.readOffset(mq2, ReadOffsetType::READ_FROM_MEMORY));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "RemoteBrokerOffsetStoreTest.*";
  return RUN_ALL_TESTS();
}
