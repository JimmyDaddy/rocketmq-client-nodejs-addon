#include <gtest/gtest.h>

#include <thread>

#include "producer/DefaultMQProducerConfigImpl.hpp"

using rocketmq::DefaultMQProducerConfigImpl;

TEST(DefaultMQProducerConfigImplTest, DefaultsMatchDocumentation) {
  DefaultMQProducerConfigImpl config;

  const int expectedThreads = std::min(4, static_cast<int>(std::thread::hardware_concurrency()));
  EXPECT_EQ(expectedThreads, config.async_send_thread_nums());
  EXPECT_EQ(4 * 1024 * 1024, config.max_message_size());
  EXPECT_EQ(4 * 1024, config.compress_msg_body_over_howmuch());
  EXPECT_EQ(5, config.compress_level());
  EXPECT_EQ(3000, config.send_msg_timeout());
  EXPECT_EQ(2, config.retry_times());
  EXPECT_EQ(2, config.retry_times_for_async());
  EXPECT_FALSE(config.retry_another_broker_when_not_store_ok());
}

TEST(DefaultMQProducerConfigImplTest, CompressLevelAcceptsOnlyValidRangeOrDisabled) {
  DefaultMQProducerConfigImpl config;

  config.set_compress_level(7);
  EXPECT_EQ(7, config.compress_level());

  config.set_compress_level(-1);
  EXPECT_EQ(-1, config.compress_level());

  config.set_compress_level(11);
  EXPECT_EQ(-1, config.compress_level());

  config.set_compress_level(-5);
  EXPECT_EQ(-1, config.compress_level());
}

TEST(DefaultMQProducerConfigImplTest, RetryCountsAreClampedBetweenZeroAndFifteen) {
  DefaultMQProducerConfigImpl config;

  config.set_retry_times(-3);
  EXPECT_EQ(0, config.retry_times());
  config.set_retry_times(99);
  EXPECT_EQ(15, config.retry_times());

  config.set_retry_times_for_async(-10);
  EXPECT_EQ(0, config.retry_times_for_async());
  config.set_retry_times_for_async(19);
  EXPECT_EQ(15, config.retry_times_for_async());
}

TEST(DefaultMQProducerConfigImplTest, BasicSettersUpdateValuesAsIs) {
  DefaultMQProducerConfigImpl config;

  config.set_async_send_thread_nums(12);
  EXPECT_EQ(12, config.async_send_thread_nums());

  config.set_max_message_size(1024);
  EXPECT_EQ(1024, config.max_message_size());

  config.set_compress_msg_body_over_howmuch(2048);
  EXPECT_EQ(2048, config.compress_msg_body_over_howmuch());

  config.set_send_msg_timeout(1234);
  EXPECT_EQ(1234, config.send_msg_timeout());

  config.set_retry_another_broker_when_not_store_ok(true);
  EXPECT_TRUE(config.retry_another_broker_when_not_store_ok());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "DefaultMQProducerConfigImplTest.*";
  return RUN_ALL_TESTS();
}
