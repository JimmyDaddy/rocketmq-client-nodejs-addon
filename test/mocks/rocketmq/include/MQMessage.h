#ifndef ROCKETMQ_STUB_MQMESSAGE_H
#define ROCKETMQ_STUB_MQMESSAGE_H

#include <cstdint>
#include <string>

namespace rocketmq {

class MQMessage {
 public:
  MQMessage();
  MQMessage(const std::string& topic, const std::string& body);

  const std::string& topic() const;
  const std::string& body() const;
  const std::string& tags() const;
  const std::string& keys() const;

  void set_tags(const std::string& tags);
  void set_keys(const std::string& keys);

 protected:
  std::string topic_;
  std::string body_;
  std::string tags_;
  std::string keys_;
};

class MQMessageExt : public MQMessage {
 public:
  MQMessageExt();
  MQMessageExt(const std::string& topic, const std::string& body);

  const std::string& msg_id() const;
  int64_t born_timestamp() const;
  int64_t store_timestamp() const;
  int32_t queue_id() const;
  int64_t queue_offset() const;
  int32_t reconsume_times() const;

  void set_msg_id(const std::string& msg_id);
  void set_born_timestamp(int64_t ts);
  void set_store_timestamp(int64_t ts);
  void set_queue_id(int32_t id);
  void set_queue_offset(int64_t offset);
  void set_reconsume_times(int32_t times);

 private:
  std::string msg_id_;
  int64_t born_timestamp_;
  int64_t store_timestamp_;
  int32_t queue_id_;
  int64_t queue_offset_;
  int32_t reconsume_times_;
};

}

#endif
