#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "MQMessageQueue.h"
#include "consumer/AssignedMessageQueue.hpp"

using rocketmq::AssignedMessageQueue;
using rocketmq::MQMessageQueue;

namespace {

std::vector<MQMessageQueue> MakeQueues(int count, const std::string& topic = "Topic") {
  std::vector<MQMessageQueue> queues;
  for (int i = 0; i < count; ++i) {
    queues.emplace_back(topic, "broker", i);
  }
  return queues;
}

}  // namespace

TEST(AssignedMessageQueueTest, AddsAndListsQueues) {
  AssignedMessageQueue assigned;
  auto queues = MakeQueues(2);

  assigned.updateAssignedMessageQueue("Topic", queues);
  auto snapshot = assigned.messageQueues();

  ASSERT_EQ(2u, snapshot.size());
  EXPECT_TRUE(std::find(snapshot.begin(), snapshot.end(), queues[0]) != snapshot.end());
  EXPECT_FALSE(assigned.isPaused(queues[0]));
}

TEST(AssignedMessageQueueTest, PauseAndResumeFlipState) {
  AssignedMessageQueue assigned;
  auto queues = MakeQueues(1);
  assigned.updateAssignedMessageQueue("Topic", queues);

  EXPECT_FALSE(assigned.isPaused(queues[0]));
  assigned.pause(queues);
  EXPECT_TRUE(assigned.isPaused(queues[0]));
  assigned.resume(queues);
  EXPECT_FALSE(assigned.isPaused(queues[0]));
}

TEST(AssignedMessageQueueTest, TracksOffsetsAndSeekValues) {
  AssignedMessageQueue assigned;
  auto queues = MakeQueues(1);
  assigned.updateAssignedMessageQueue("Topic", queues);
  const auto& queue = queues[0];

  EXPECT_EQ(-1, assigned.getPullOffset(queue));
  EXPECT_EQ(-1, assigned.getConsumerOffset(queue));
  EXPECT_EQ(-1, assigned.getSeekOffset(queue));

  assigned.updatePullOffset(queue, 100);
  assigned.updateConsumeOffset(queue, 80);
  assigned.setSeekOffset(queue, 50);

  EXPECT_EQ(100, assigned.getPullOffset(queue));
  EXPECT_EQ(80, assigned.getConsumerOffset(queue));
  EXPECT_EQ(50, assigned.getSeekOffset(queue));
}

TEST(AssignedMessageQueueTest, UpdateAssignedRemovesStaleQueues) {
  AssignedMessageQueue assigned;
  auto queues = MakeQueues(2);
  assigned.updateAssignedMessageQueue("Topic", queues);

  std::vector<MQMessageQueue> newAssignment = {queues[1]};
  assigned.updateAssignedMessageQueue("Topic", newAssignment);

  auto snapshot = assigned.messageQueues();
  ASSERT_EQ(1u, snapshot.size());
  EXPECT_EQ(queues[1], snapshot[0]);
  // Removed queue becomes paused by default because it's no longer tracked.
  EXPECT_TRUE(assigned.isPaused(queues[0]));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "AssignedMessageQueueTest.*";
  return RUN_ALL_TESTS();
}
