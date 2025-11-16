#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

#include <spdlog/sinks/rotating_file_sink.h>

#include "Logging.h"

using rocketmq::GetDefaultLogger;
using rocketmq::GetDefaultLoggerConfig;
using rocketmq::LogLevel;
using rocketmq::Logger;

namespace {

std::string MakeTempPath(const std::string& tag) {
  std::string pattern = "/tmp/rocketmq_logging_" + tag + "XXXXXX";
  std::vector<char> buffer(pattern.begin(), pattern.end());
  buffer.push_back('\0');
  int fd = mkstemp(buffer.data());
  if (fd < 0) {
    throw std::runtime_error("mkstemp failed");
  }
  close(fd);
  return std::string(buffer.data());
}

std::string ReadFile(const std::string& path) {
  std::ifstream stream(path);
  return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool WaitForSubstring(const std::string& path, const std::string& needle) {
  for (int attempt = 0; attempt < 25; ++attempt) {
    auto contents = ReadFile(path);
    if (contents.find(needle) != std::string::npos) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return false;
}

}  // namespace

class LoggingTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    default_log_path_ = MakeTempPath("default");
    default_logger_name_ = "default-test-logger-" + std::to_string(getpid());
    auto& config = GetDefaultLoggerConfig();
    config.set_name(default_logger_name_);
    config.set_path(default_log_path_);
    config.set_level(LogLevel::LOG_LEVEL_DEBUG);
    config.set_file_size(1024 * 1024);
    config.set_file_count(1);
    config.set_config_spdlog(true);
  }

  static void TearDownTestSuite() {
    std::remove(default_log_path_.c_str());
  }

  static std::string default_log_path_;
  static std::string default_logger_name_;
};

std::string LoggingTest::default_log_path_;
std::string LoggingTest::default_logger_name_;

TEST_F(LoggingTest, MacroLoggingWritesToConfiguredFile) {
  auto& logger = GetDefaultLogger();
  (void)logger;

  LOG_INFO("macro message %d", 7);
  auto backend = spdlog::get(default_logger_name_);
  ASSERT_NE(nullptr, backend);
  backend->flush();
  ASSERT_FALSE(backend->sinks().empty());
  auto rotating_sink = std::dynamic_pointer_cast<spdlog::sinks::rotating_file_sink_mt>(backend->sinks().front());
  ASSERT_NE(nullptr, rotating_sink);
  EXPECT_EQ(default_log_path_, rotating_sink->filename());

  EXPECT_TRUE(WaitForSubstring(default_log_path_, "macro message 7"));
}

TEST_F(LoggingTest, CustomLoggerPrintfWritesToTargetPath) {
  auto custom_log_path = MakeTempPath("custom");
  {
    Logger logger({"custom-logger", LogLevel::LOG_LEVEL_TRACE, custom_log_path, 1024 * 1024, 1});
    logger.DebugPrintf(LOG_SOURCE_LOCATION, "value=%d", 42);
    logger.ErrorPrintf(LOG_SOURCE_LOCATION, "structured %s", "message");
    auto backend = spdlog::get("custom-logger");
    ASSERT_NE(nullptr, backend);
    backend->flush();
  }

  EXPECT_TRUE(WaitForSubstring(custom_log_path, "value=42"));
  EXPECT_TRUE(WaitForSubstring(custom_log_path, "structured message"));
  std::remove(custom_log_path.c_str());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "LoggingTest.*";
  return RUN_ALL_TESTS();
}
