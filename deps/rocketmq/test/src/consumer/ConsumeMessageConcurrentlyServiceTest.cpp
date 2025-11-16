#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <tuple>
#include <vector>

#define private public
#define protected public
#include "consumer/DefaultMQPushConsumerImpl.h"
#include "consumer/ConsumeMsgService.h"
#undef private
#undef protected

#include "consumer/DefaultMQPushConsumerConfigImpl.hpp"
#include "consumer/ProcessQueue.h"
#include "message/MessageExtImpl.h"
#include "OffsetStore.h"

namespace rocketmq {
namespace {

class RecordingOffsetStore : public OffsetStore {
 public:
  void load() override {}
  void updateOffset(const MQMessageQueue& mq, int64_t offset, bool increaseOnly) override {
    ++update_calls;
    last_mq = mq;
    last_offset = offset;
    last_increase_only = increaseOnly;
  }
  int64_t readOffset(const MQMessageQueue&, ReadOffsetType) override { return -1; }
  void persist(const MQMessageQueue&) override {}
  void persistAll(std::vector<MQMessageQueue>&) override {}
  void removeOffset(const MQMessageQueue&) override {}

  int update_calls = 0;
  MQMessageQueue last_mq;
  int64_t last_offset = -1;
  bool last_increase_only = false;
};

class StubMessageListener : public MessageListenerConcurrently {
 public:
  explicit StubMessageListener(ConsumeStatus status) : status_(status) {}

  ConsumeStatus consumeMessage(std::vector<MQMessageExt>& msgs) override {
    batches.push_back(msgs);
    return status_;
  }

  ConsumeStatus status_;
  std::vector<std::vector<MQMessageExt>> batches;
};

class StubDefaultMQPushConsumerImpl : public DefaultMQPushConsumerImpl {
 public:
  explicit StubDefaultMQPushConsumerImpl(DefaultMQPushConsumerConfigPtr config)
      : DefaultMQPushConsumerImpl(config) {}

  bool sendMessageBack(MessageExtPtr msg, int delayLevel) override {
    return sendMessageBack(msg, delayLevel, "");
  }

  bool sendMessageBack(MessageExtPtr msg, int delayLevel, const std::string& brokerName) override {
    send_back_calls.emplace_back(msg, delayLevel, brokerName);
    bool result = default_send_result;
    if (!next_send_results.empty()) {
      result = next_send_results.front();
      next_send_results.pop_front();
    }
    return result;
  }

  using SendBackCall = std::tuple<MessageExtPtr, int, std::string>;
  std::vector<SendBackCall> send_back_calls;
  std::deque<bool> next_send_results;
  bool default_send_result = true;
};

DefaultMQPushConsumerConfigPtr makeConfig() {
  auto config = std::make_shared<DefaultMQPushConsumerConfigImpl>();
  config->set_group_name("GroupA");
  config->set_message_model(MessageModel::CLUSTERING);
  config->set_consume_thread_nums(1);
  return config;
}

std::vector<MessageExtPtr> makeMessages(const std::vector<int64_t>& offsets) {
  std::vector<MessageExtPtr> msgs;
  for (const auto& offset : offsets) {
    auto msg = std::make_shared<MessageExtImpl>();
    msg->set_queue_offset(offset);
    msg->set_msg_id("MSG" + std::to_string(offset));
    msg->set_topic("TestTopic");
    msgs.emplace_back(msg);
  }
  return msgs;
}

TEST(ConsumeMessageConcurrentlyServiceTest, ConsumeSuccessUpdatesOffset) {
  auto config = makeConfig();
  StubDefaultMQPushConsumerImpl consumer(config);
  std::unique_ptr<RecordingOffsetStore> store(new RecordingOffsetStore());
  auto* store_ptr = store.get();
  consumer.offset_store_ = std::move(store);

  StubMessageListener listener(CONSUME_SUCCESS);
  ConsumeMessageConcurrentlyService service(&consumer, 1, &listener);

  auto processQueue = std::make_shared<ProcessQueue>();
  MQMessageQueue mq("TestTopic", "TestBroker", 3);
  auto msgs = makeMessages({0, 1, 2});
  processQueue->putMessage(msgs);

  service.ConsumeRequest(msgs, processQueue, mq);

  EXPECT_EQ(1, store_ptr->update_calls);
  EXPECT_EQ(3, store_ptr->last_offset);
  EXPECT_EQ(mq, store_ptr->last_mq);
  EXPECT_TRUE(store_ptr->last_increase_only);
  EXPECT_TRUE(consumer.send_back_calls.empty());
  ASSERT_EQ(1u, listener.batches.size());
  EXPECT_EQ(3u, listener.batches.front().size());
}

TEST(ConsumeMessageConcurrentlyServiceTest, ReconsumeLaterSendsBackAndAdvancesToPendingOffset) {
  auto config = makeConfig();
  StubDefaultMQPushConsumerImpl consumer(config);
  std::unique_ptr<RecordingOffsetStore> store(new RecordingOffsetStore());
  auto* store_ptr = store.get();
  consumer.offset_store_ = std::move(store);
  consumer.next_send_results = {true, false};

  StubMessageListener listener(RECONSUME_LATER);
  ConsumeMessageConcurrentlyService service(&consumer, 1, &listener);

  auto processQueue = std::make_shared<ProcessQueue>();
  MQMessageQueue mq("TestTopic", "TestBroker", 7);
  auto msgs = makeMessages({10, 11});
  processQueue->putMessage(msgs);

  service.ConsumeRequest(msgs, processQueue, mq);

  ASSERT_EQ(2u, consumer.send_back_calls.size());
  EXPECT_EQ(1u, msgs.size());
  EXPECT_EQ(11, std::get<0>(consumer.send_back_calls.back())->queue_offset());
  EXPECT_EQ(1, store_ptr->update_calls);
  EXPECT_EQ(11, store_ptr->last_offset);
  EXPECT_EQ(1, processQueue->getCacheMsgCount());
  EXPECT_EQ(11, processQueue->getCacheMinOffset());
}

}  // namespace
}  // namespace rocketmq
