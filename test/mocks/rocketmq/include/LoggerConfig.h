#ifndef ROCKETMQ_STUB_LOGGERCONFIG_H
#define ROCKETMQ_STUB_LOGGERCONFIG_H

#include <cstdint>
#include <string>

namespace rocketmq {

enum LogLevel {
  LOG_LEVEL_FATAL = 1,
  LOG_LEVEL_ERROR = 2,
  LOG_LEVEL_WARN = 3,
  LOG_LEVEL_INFO = 4,
  LOG_LEVEL_DEBUG = 5,
  LOG_LEVEL_TRACE = 6,
  LOG_LEVEL_LEVEL_NUM = 7
};

class LoggerConfig {
 public:
  void set_level(LogLevel level);
  void set_path(const std::string& path);
  void set_file_size(int64_t size);
  void set_file_count(int count);
};

LoggerConfig& GetDefaultLoggerConfig();

}

#endif
