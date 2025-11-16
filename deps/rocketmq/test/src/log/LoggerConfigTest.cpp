#include <gtest/gtest.h>

#include <string>

#include "LoggerConfig.h"

using rocketmq::LogLevel;
using rocketmq::LoggerConfig;

TEST(LoggerConfigTest, DefaultConstructorInitializesInfoLevelAndRotation) {
  LoggerConfig config("test", "/tmp/rocketmq.log");

  EXPECT_EQ("test", config.name());
  EXPECT_EQ(LogLevel::LOG_LEVEL_INFO, config.level());
  EXPECT_EQ("/tmp/rocketmq.log", config.path());
  EXPECT_EQ(1024 * 1024 * 100, config.file_size());
  EXPECT_EQ(3, config.file_count());
  EXPECT_TRUE(config.config_spdlog());
}

TEST(LoggerConfigTest, SetterGetterRoundTrip) {
  LoggerConfig config("initial", "/tmp/init.log");

  config.set_name("custom");
  config.set_level(LogLevel::LOG_LEVEL_DEBUG);
  config.set_path("/var/log/custom.log");
  config.set_file_size(4096);
  config.set_file_count(10);
  config.set_config_spdlog(false);

  EXPECT_EQ("custom", config.name());
  EXPECT_EQ(LogLevel::LOG_LEVEL_DEBUG, config.level());
  EXPECT_EQ("/var/log/custom.log", config.path());
  EXPECT_EQ(4096, config.file_size());
  EXPECT_EQ(10, config.file_count());
  EXPECT_FALSE(config.config_spdlog());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "LoggerConfigTest.*";
  return RUN_ALL_TESTS();
}
