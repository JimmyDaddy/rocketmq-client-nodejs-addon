#ifndef ROCKETMQ_STUB_DEFAULT_MQ_PRODUCER_H
#define ROCKETMQ_STUB_DEFAULT_MQ_PRODUCER_H

#include <memory>
#include <string>

#include "ClientRPCHook.h"
#include "MQMessage.h"
#include "SendCallback.h"

namespace rocketmq {

class DefaultMQProducer {
 public:
  explicit DefaultMQProducer(const std::string& group_name);

  void set_group_name(const std::string& group_name);
  void set_instance_name(const std::string& instance_name);
  void set_namesrv_addr(const std::string& namesrv_addr);
  void set_max_message_size(int max_message_size);
  void set_compress_level(int compress_level);
  void set_send_msg_timeout(int timeout_ms);
  void setRPCHook(std::shared_ptr<ClientRPCHook> rpc_hook);

  void start();
  void shutdown();
  void send(MQMessage& message, AutoDeleteSendCallback* callback);

 private:
  std::string group_name_;
  std::string instance_name_;
  std::string namesrv_addr_;
  int max_message_size_;
  int compress_level_;
  int send_msg_timeout_;
  std::shared_ptr<ClientRPCHook> rpc_hook_;
};

}

#endif
