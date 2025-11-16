#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "MQMessageExt.h"
#include "consumer/ProcessQueue.h"
#include "protocol/body/ProcessQueueInfo.hpp"

using rocketmq::MessageExtPtr;
using rocketmq::MQMessageExt;
using rocketmq::ProcessQueue;
using rocketmq::ProcessQueueInfo;

namespace {

MessageExtPtr MakeMessage(int64_t offset, int queue_id = 0) {
  auto msg = std::make_shared<MQMessageExt>();
  msg->set_queue_offset(offset);
  msg->set_queue_id(queue_id);
  return msg;
}

}  // namespace

TEST(ProcessQueueTest, PutAndRemoveMessagesTrackOffsets) {
  ProcessQueue pq;
  std::vector<MessageExtPtr> msgs = {MakeMessage(5), MakeMessage(6), MakeMessage(8)};
  pq.putMessage(msgs);

  EXPECT_EQ(3, pq.getCacheMsgCount());
  EXPECT_EQ(5, pq.getCacheMinOffset());
  EXPECT_EQ(8, pq.getCacheMaxOffset());

  std::vector<MessageExtPtr> removed = {MakeMessage(5)};
  EXPECT_EQ(6, pq.removeMessage(removed));
  EXPECT_EQ(2, pq.getCacheMsgCount());
  EXPECT_EQ(6, pq.getCacheMinOffset());
}

TEST(ProcessQueueTest, TakeMessagesAndRequeueUpdatesCaches) {
  ProcessQueue pq;
  pq.putMessage({MakeMessage(1), MakeMessage(2), MakeMessage(3)});

  std::vector<MessageExtPtr> batch;
  pq.takeMessages(batch, 2);
  ASSERT_EQ(2u, batch.size());
  EXPECT_EQ(3, pq.getCacheMsgCount());
  EXPECT_EQ(1, pq.getCacheMinOffset());

  pq.makeMessageToCosumeAgain(batch);
  EXPECT_EQ(3, pq.getCacheMsgCount());
  EXPECT_EQ(1, pq.getCacheMinOffset());
}

TEST(ProcessQueueTest, CommitFlushesConsumingSet) {
  ProcessQueue pq;
  pq.putMessage({MakeMessage(10), MakeMessage(11)});

  std::vector<MessageExtPtr> batch;
  pq.takeMessages(batch, 2);
  ASSERT_EQ(2u, batch.size());

  EXPECT_EQ(12, pq.commit());
  EXPECT_EQ(0, pq.getCacheMsgCount());
  EXPECT_EQ(-1, pq.commit());
}

TEST(ProcessQueueTest, ClearAllMsgsRequiresDroppedFlag) {
  ProcessQueue pq;
  pq.putMessage({MakeMessage(2)});

  pq.clearAllMsgs();
  EXPECT_EQ(1, pq.getCacheMsgCount());

  pq.set_dropped(true);
  pq.clearAllMsgs();
  EXPECT_EQ(0, pq.getCacheMsgCount());
  EXPECT_EQ(0, pq.getCacheMaxOffset());
}

TEST(ProcessQueueTest, FillProcessQueueInfoReflectsState) {
  ProcessQueue pq;
  pq.putMessage({MakeMessage(7), MakeMessage(8)});

  std::vector<MessageExtPtr> batch;
  pq.takeMessages(batch, 1);

  pq.set_locked(true);
  pq.set_dropped(true);
  pq.set_last_pull_timestamp(5000);
  pq.set_last_consume_timestamp(6000);
  pq.set_last_lock_timestamp(12345);
  pq.inc_try_unlock_times();
  auto expected_unlocks = pq.try_unlock_times();

  ProcessQueueInfo info;
  pq.fillProcessQueueInfo(info);

  EXPECT_EQ(8, info.cachedMsgMinOffset);
  EXPECT_EQ(8, info.cachedMsgMaxOffset);
  EXPECT_EQ(1, info.cachedMsgCount);

  EXPECT_EQ(7, info.transactionMsgMinOffset);
  EXPECT_EQ(7, info.transactionMsgMaxOffset);
  EXPECT_EQ(1, info.transactionMsgCount);

  EXPECT_TRUE(info.isLocked());
  EXPECT_TRUE(info.isDroped());
  EXPECT_EQ(static_cast<int32_t>(expected_unlocks), info.tryUnlockTimes);
  EXPECT_EQ(12345u, info.lastLockTimestamp);
  EXPECT_EQ(5000u, info.lastPullTimestamp);
  EXPECT_EQ(6000u, info.lastConsumeTimestamp);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ProcessQueueTest.*";
  return RUN_ALL_TESTS();
}
