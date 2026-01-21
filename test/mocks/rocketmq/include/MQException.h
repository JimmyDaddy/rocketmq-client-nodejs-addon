#ifndef ROCKETMQ_STUB_MQEXCEPTION_H
#define ROCKETMQ_STUB_MQEXCEPTION_H

#include <exception>
#include <string>

namespace rocketmq {

class MQException : public std::exception {
 public:
  explicit MQException(const std::string& message);
  const char* what() const noexcept override;

 private:
  std::string message_;
};

}

#endif
