#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "MQException.h"
#include "MQMessageQueue.h"
#include "consumer/AllocateMQAveragely.hpp"

using rocketmq::AllocateMQAveragely;
using rocketmq::MQException;
using rocketmq::MQMessageQueue;

namespace {

std::vector<MQMessageQueue> MakeQueues(int count) {
  std::vector<MQMessageQueue> queues;
  queues.reserve(count);
  for (int i = 0; i < count; ++i) {
    queues.emplace_back("TestTopic", "brokerA", i);
  }
  return queues;
}

}  // namespace

TEST(AllocateMQAveragelyTest, ThrowsWhenRequiredInputsMissing) {
  AllocateMQAveragely allocator;
  auto queues = MakeQueues(2);
  std::vector<std::string> consumers = {"cid"};
  std::vector<MQMessageQueue> result;

  EXPECT_THROW(allocator.allocate("", queues, consumers, result), MQException);
  std::vector<MQMessageQueue> emptyQueues;
  EXPECT_THROW(allocator.allocate("cid", emptyQueues, consumers, result), MQException);
  std::vector<std::string> emptyConsumers;
  EXPECT_THROW(allocator.allocate("cid", queues, emptyConsumers, result), MQException);
}

TEST(AllocateMQAveragelyTest, DistributesQueuesEvenlyAcrossConsumers) {
  AllocateMQAveragely allocator;
  auto queues = MakeQueues(7);
  std::vector<std::string> consumers = {"c0", "c1", "c2"};
  std::vector<MQMessageQueue> result;

  allocator.allocate("c0", queues, consumers, result);
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ(0, result[0].queue_id());
  EXPECT_EQ(1, result[1].queue_id());
  EXPECT_EQ(2, result[2].queue_id());

  allocator.allocate("c1", queues, consumers, result);
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(3, result[0].queue_id());
  EXPECT_EQ(4, result[1].queue_id());

  allocator.allocate("c2", queues, consumers, result);
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(5, result[0].queue_id());
  EXPECT_EQ(6, result[1].queue_id());
}

TEST(AllocateMQAveragelyTest, HandlesMoreConsumersThanQueuesGracefully) {
  AllocateMQAveragely allocator;
  auto queues = MakeQueues(2);
  std::vector<std::string> consumers = {"c0", "c1", "c2", "c3"};
  std::vector<MQMessageQueue> result;

  allocator.allocate("c0", queues, consumers, result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(0, result[0].queue_id());

  allocator.allocate("c1", queues, consumers, result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(1, result[0].queue_id());

  allocator.allocate("c2", queues, consumers, result);
  EXPECT_TRUE(result.empty());

  allocator.allocate("missing", queues, consumers, result);
  EXPECT_TRUE(result.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "AllocateMQAveragelyTest.*";
  return RUN_ALL_TESTS();
}
