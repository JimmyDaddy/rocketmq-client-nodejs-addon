#include <gtest/gtest.h>

#include <thread>

#include "AllocateMQStrategy.h"
#include "ConsumeType.h"
#include "consumer/DefaultMQPushConsumerConfigImpl.hpp"

namespace {

class CountingStrategy : public rocketmq::AllocateMQStrategy {
 public:
  CountingStrategy() { ++instances; }
  ~CountingStrategy() override { ++destroyed; }

  void allocate(const std::string&,
                std::vector<rocketmq::MQMessageQueue>&,
                std::vector<std::string>&,
                std::vector<rocketmq::MQMessageQueue>&) override {}

  static void Reset() {
    instances = 0;
    destroyed = 0;
  }

  static int instances;
  static int destroyed;
};

int CountingStrategy::instances = 0;
int CountingStrategy::destroyed = 0;

}  // namespace

using rocketmq::AllocateMQAveragely;
using rocketmq::DefaultMQPushConsumerConfigImpl;

TEST(DefaultMQPushConsumerConfigImplTest, DefaultsMatchExpectations) {
  DefaultMQPushConsumerConfigImpl config;

  EXPECT_EQ(rocketmq::MessageModel::CLUSTERING, config.message_model());
  EXPECT_EQ(rocketmq::ConsumeFromWhere::CONSUME_FROM_LAST_OFFSET, config.consume_from_where());
  EXPECT_EQ("0", config.consume_timestamp());
  const int expected_threads = std::min(8, static_cast<int>(std::thread::hardware_concurrency()));
  EXPECT_EQ(expected_threads, config.consume_thread_nums());
  EXPECT_EQ(1000, config.pull_threshold_for_queue());
  EXPECT_EQ(1, config.consume_message_batch_max_size());
  EXPECT_EQ(32, config.pull_batch_size());
  EXPECT_EQ(16, config.max_reconsume_times());
  EXPECT_EQ(3000, config.pull_time_delay_millis_when_exception());
  ASSERT_NE(nullptr, config.allocate_mq_strategy());
  EXPECT_NE(nullptr, dynamic_cast<AllocateMQAveragely*>(config.allocate_mq_strategy()));
}

TEST(DefaultMQPushConsumerConfigImplTest, SetterGuardsAreEnforced) {
  DefaultMQPushConsumerConfigImpl config;

  config.set_message_model(rocketmq::MessageModel::BROADCASTING);
  EXPECT_EQ(rocketmq::MessageModel::BROADCASTING, config.message_model());

  config.set_consume_from_where(rocketmq::ConsumeFromWhere::CONSUME_FROM_TIMESTAMP);
  EXPECT_EQ(rocketmq::ConsumeFromWhere::CONSUME_FROM_TIMESTAMP, config.consume_from_where());

  config.set_consume_timestamp("123");
  EXPECT_EQ("123", config.consume_timestamp());

  config.set_consume_thread_nums(12);
  EXPECT_EQ(12, config.consume_thread_nums());
  config.set_consume_thread_nums(0);  // should be ignored
  EXPECT_EQ(12, config.consume_thread_nums());

  config.set_pull_threshold_for_queue(2500);
  EXPECT_EQ(2500, config.pull_threshold_for_queue());

  config.set_consume_message_batch_max_size(5);
  EXPECT_EQ(5, config.consume_message_batch_max_size());
  config.set_consume_message_batch_max_size(0);
  EXPECT_EQ(5, config.consume_message_batch_max_size());

  config.set_pull_batch_size(64);
  EXPECT_EQ(64, config.pull_batch_size());

  config.set_max_reconsume_times(3);
  EXPECT_EQ(3, config.max_reconsume_times());

  config.set_pull_time_delay_millis_when_exception(1500);
  EXPECT_EQ(1500, config.pull_time_delay_millis_when_exception());
}

TEST(DefaultMQPushConsumerConfigImplTest, StrategySetterTransfersOwnership) {
  CountingStrategy::Reset();
  DefaultMQPushConsumerConfigImpl config;

  auto* strategy_one = new CountingStrategy();
  config.set_allocate_mq_strategy(strategy_one);
  EXPECT_EQ(strategy_one, config.allocate_mq_strategy());
  EXPECT_EQ(1, CountingStrategy::instances);
  EXPECT_EQ(0, CountingStrategy::destroyed);

  auto* strategy_two = new CountingStrategy();
  config.set_allocate_mq_strategy(strategy_two);
  EXPECT_EQ(strategy_two, config.allocate_mq_strategy());
  EXPECT_EQ(2, CountingStrategy::instances);
  EXPECT_EQ(1, CountingStrategy::destroyed);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "DefaultMQPushConsumerConfigImplTest.*";
  return RUN_ALL_TESTS();
}
