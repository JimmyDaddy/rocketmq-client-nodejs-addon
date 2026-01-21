#ifndef ROCKETMQ_STUB_MQMESSAGE_LISTENER_H
#define ROCKETMQ_STUB_MQMESSAGE_LISTENER_H

#include <vector>

#include "MQMessage.h"

namespace rocketmq {

enum ConsumeStatus {
  CONSUME_SUCCESS = 0,
  RECONSUME_LATER = 1
};

class MessageListenerConcurrently {
 public:
  virtual ~MessageListenerConcurrently();
  virtual ConsumeStatus consumeMessage(std::vector<MQMessageExt>& msgs) = 0;
};

}

#endif
