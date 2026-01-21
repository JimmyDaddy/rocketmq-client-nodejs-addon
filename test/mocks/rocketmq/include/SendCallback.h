#ifndef ROCKETMQ_STUB_SENDCALLBACK_H
#define ROCKETMQ_STUB_SENDCALLBACK_H

#include <cstdint>
#include <string>

namespace rocketmq {

enum SendStatus {
  SEND_OK = 0,
  FLUSH_DISK_TIMEOUT = 1,
  FLUSH_SLAVE_TIMEOUT = 2,
  SLAVE_NOT_AVAILABLE = 3
};

class SendResult {
 public:
  SendResult();
  SendResult(SendStatus status, const std::string& msg_id, int64_t queue_offset);
  SendStatus send_status() const;
  const std::string& msg_id() const;
  int64_t queue_offset() const;

 private:
  SendStatus status_;
  std::string msg_id_;
  int64_t queue_offset_;
};

class MQException;

class SendCallback {
 public:
  virtual ~SendCallback();
  virtual void onSuccess(SendResult& send_result) = 0;
  virtual void onException(MQException& exception) noexcept = 0;
};

class AutoDeleteSendCallback : public SendCallback {
 public:
  ~AutoDeleteSendCallback() override;
};

}

#endif
