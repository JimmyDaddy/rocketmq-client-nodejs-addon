#include <gtest/gtest.h>

#include <string>

#include "AllocateMQStrategy.h"
#include "ConsumeType.h"
#include "consumer/DefaultLitePullConsumerConfigImpl.hpp"

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
using rocketmq::DefaultLitePullConsumerConfigImpl;

TEST(DefaultLitePullConsumerConfigImplTest, DefaultsMatchExpectations) {
  DefaultLitePullConsumerConfigImpl config;

  EXPECT_EQ(rocketmq::MessageModel::CLUSTERING, config.message_model());
  EXPECT_EQ(rocketmq::ConsumeFromWhere::CONSUME_FROM_LAST_OFFSET, config.consume_from_where());
  EXPECT_FALSE(config.consume_timestamp().empty());
  EXPECT_EQ(5000, config.auto_commit_interval_millis());
  EXPECT_EQ(10, config.pull_batch_size());
  EXPECT_EQ(20, config.pull_thread_nums());
  EXPECT_TRUE(config.long_polling_enable());
  EXPECT_EQ(10000, config.consumer_pull_timeout_millis());
  EXPECT_EQ(30000, config.consumer_timeout_millis_when_suspend());
  EXPECT_EQ(20000, config.broker_suspend_max_time_millis());
  EXPECT_EQ(10000, config.pull_threshold_for_all());
  EXPECT_EQ(1000, config.pull_threshold_for_queue());
  EXPECT_EQ(1000, config.pull_time_delay_millis_when_exception());
  EXPECT_EQ(5000, config.poll_timeout_millis());
  EXPECT_EQ(30000, config.topic_metadata_check_interval_millis());
  ASSERT_NE(nullptr, config.allocate_mq_strategy());
  EXPECT_NE(nullptr, dynamic_cast<AllocateMQAveragely*>(config.allocate_mq_strategy()));
}

TEST(DefaultLitePullConsumerConfigImplTest, SettersApplyNewValues) {
  DefaultLitePullConsumerConfigImpl config;

  config.set_message_model(rocketmq::MessageModel::BROADCASTING);
  config.set_consume_from_where(rocketmq::ConsumeFromWhere::CONSUME_FROM_FIRST_OFFSET);
  config.set_consume_timestamp("123456");
  config.set_auto_commit_interval_millis(123);
  config.set_pull_batch_size(99);
  config.set_pull_thread_nums(7);
  config.set_long_polling_enable(false);
  config.set_consumer_pull_timeout_millis(2222);
  config.set_consumer_timeout_millis_when_suspend(3333);
  config.set_broker_suspend_max_time_millis(4444);
  config.set_pull_threshold_for_all(5555);
  config.set_pull_threshold_for_queue(666);
  config.set_pull_time_delay_millis_when_exception(777);
  config.set_poll_timeout_millis(888);
  config.set_topic_metadata_check_interval_millis(999);

  EXPECT_EQ(rocketmq::MessageModel::BROADCASTING, config.message_model());
  EXPECT_EQ(rocketmq::ConsumeFromWhere::CONSUME_FROM_FIRST_OFFSET, config.consume_from_where());
  EXPECT_EQ("123456", config.consume_timestamp());
  EXPECT_EQ(123, config.auto_commit_interval_millis());
  EXPECT_EQ(99, config.pull_batch_size());
  EXPECT_EQ(7, config.pull_thread_nums());
  EXPECT_FALSE(config.long_polling_enable());
  EXPECT_EQ(2222, config.consumer_pull_timeout_millis());
  EXPECT_EQ(3333, config.consumer_timeout_millis_when_suspend());
  EXPECT_EQ(4444, config.broker_suspend_max_time_millis());
  EXPECT_EQ(5555, config.pull_threshold_for_all());
  EXPECT_EQ(666, config.pull_threshold_for_queue());
  EXPECT_EQ(777, config.pull_time_delay_millis_when_exception());
  EXPECT_EQ(888, config.poll_timeout_millis());
  EXPECT_EQ(999, config.topic_metadata_check_interval_millis());
}

TEST(DefaultLitePullConsumerConfigImplTest, StrategySetterTransfersOwnership) {
  CountingStrategy::Reset();
  DefaultLitePullConsumerConfigImpl config;

  auto* first = new CountingStrategy();
  config.set_allocate_mq_strategy(first);
  EXPECT_EQ(first, config.allocate_mq_strategy());
  EXPECT_EQ(1, CountingStrategy::instances);
  EXPECT_EQ(0, CountingStrategy::destroyed);

  auto* second = new CountingStrategy();
  config.set_allocate_mq_strategy(second);
  EXPECT_EQ(second, config.allocate_mq_strategy());
  EXPECT_EQ(2, CountingStrategy::instances);
  EXPECT_EQ(1, CountingStrategy::destroyed);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "DefaultLitePullConsumerConfigImplTest.*";
  return RUN_ALL_TESTS();
}
