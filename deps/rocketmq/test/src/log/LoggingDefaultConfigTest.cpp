#include <gtest/gtest.h>

#include <string>

#include "Logging.h"

using rocketmq::GetDefaultLoggerConfig;
using rocketmq::LogLevel;

TEST(LoggingDefaultConfigTest, ReturnsSingletonWithExpectedDefaults) {
  auto& config = GetDefaultLoggerConfig();
  auto& config_again = GetDefaultLoggerConfig();

  EXPECT_EQ(&config, &config_again);
  EXPECT_EQ("default", config.name());
  EXPECT_EQ(LogLevel::LOG_LEVEL_INFO, config.level());
  EXPECT_EQ(1024 * 1024 * 100, config.file_size());
  EXPECT_EQ(3, config.file_count());
  EXPECT_TRUE(config.config_spdlog());
  EXPECT_FALSE(config.path().empty());
  EXPECT_NE(std::string::npos, config.path().find("rocketmq"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "LoggingDefaultConfigTest.*";
  return RUN_ALL_TESTS();
}
