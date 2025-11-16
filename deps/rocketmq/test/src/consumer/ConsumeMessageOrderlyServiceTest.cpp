#include <gtest/gtest.h>

#include <deque>
#include <memory>
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
#include "UtilAll.h"

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

class SequentialMessageListener : public MessageListenerOrderly {
 public:
  explicit SequentialMessageListener(std::vector<ConsumeStatus> statuses)
      : statuses_(std::move(statuses)), status_index_(0) {}

  ConsumeStatus consumeMessage(std::vector<MQMessageExt>& msgs) override {
    invocations.push_back(msgs);
    if (status_index_ < statuses_.size()) {
      return statuses_[status_index_++];
    }
    return CONSUME_SUCCESS;
  }

  std::vector<ConsumeStatus> statuses_;
  size_t status_index_;
  std::vector<std::vector<MQMessageExt>> invocations;
};

class StubDefaultMQPushConsumerImpl : public DefaultMQPushConsumerImpl {
 public:
  explicit StubDefaultMQPushConsumerImpl(DefaultMQPushConsumerConfigPtr config)
      : DefaultMQPushConsumerImpl(config) {}

  bool sendMessageBack(MessageExtPtr msg, int delayLevel) override {
    return sendMessageBack(msg, delayLevel, "");
  }

  bool sendMessageBack(MessageExtPtr, int, const std::string&) override { return true; }
};

DefaultMQPushConsumerConfigPtr makeConfig() {
  auto config = std::make_shared<DefaultMQPushConsumerConfigImpl>();
  config->set_group_name("GroupB");
  config->set_consume_thread_nums(1);
  config->set_consume_message_batch_max_size(16);
  config->set_message_model(MessageModel::CLUSTERING);
  return config;
}

std::vector<MessageExtPtr> makeMessages(const std::vector<int64_t>& offsets) {
  std::vector<MessageExtPtr> msgs;
  for (auto offset : offsets) {
    auto msg = std::make_shared<MessageExtImpl>();
    msg->set_queue_offset(offset);
    msg->set_msg_id("MSG" + std::to_string(offset));
    msg->set_topic("OrderedTopic");
    msgs.emplace_back(msg);
  }
  return msgs;
}

std::shared_ptr<ProcessQueue> prepareProcessQueue(const std::vector<MessageExtPtr>& msgs) {
  auto pq = std::make_shared<ProcessQueue>();
  pq->putMessage(msgs);
  pq->set_locked(true);
  pq->set_last_lock_timestamp(UtilAll::currentTimeMillis());
  return pq;
}

TEST(ConsumeMessageOrderlyServiceTest, ConsumeSuccessCommitsOffsets) {
  auto config = makeConfig();
  StubDefaultMQPushConsumerImpl consumer(config);
  std::unique_ptr<RecordingOffsetStore> store(new RecordingOffsetStore());
  auto* store_ptr = store.get();
  consumer.offset_store_ = std::move(store);

  SequentialMessageListener listener({CONSUME_SUCCESS});
  ConsumeMessageOrderlyService service(&consumer, 1, &listener);

  auto msgs = makeMessages({100, 101});
  auto processQueue = prepareProcessQueue(msgs);
  MQMessageQueue mq("OrderedTopic", "BrokerA", 0);

  service.ConsumeRequest(processQueue, mq);

  EXPECT_EQ(1, store_ptr->update_calls);
  EXPECT_EQ(102, store_ptr->last_offset);
  EXPECT_FALSE(store_ptr->last_increase_only);
  EXPECT_EQ(0, processQueue->getCacheMsgCount());
  ASSERT_EQ(1u, listener.invocations.size());
  EXPECT_EQ(2u, listener.invocations.front().size());
}

TEST(ConsumeMessageOrderlyServiceTest, ReconsumeLaterRequeuesMessagesAndSkipsOffsetCommit) {
  auto config = makeConfig();
  StubDefaultMQPushConsumerImpl consumer(config);
  std::unique_ptr<RecordingOffsetStore> store(new RecordingOffsetStore());
  auto* store_ptr = store.get();
  consumer.offset_store_ = std::move(store);

  SequentialMessageListener listener({RECONSUME_LATER});
  ConsumeMessageOrderlyService service(&consumer, 1, &listener);

  auto msgs = makeMessages({200, 201});
  auto processQueue = prepareProcessQueue(msgs);
  MQMessageQueue mq("OrderedTopic", "BrokerB", 3);

  service.ConsumeRequest(processQueue, mq);

  EXPECT_EQ(0, store_ptr->update_calls);
  EXPECT_EQ(2, processQueue->getCacheMsgCount());
  EXPECT_EQ(200, processQueue->getCacheMinOffset());
  ASSERT_EQ(1u, listener.invocations.size());
  EXPECT_EQ(2u, listener.invocations.front().size());
}

}  // namespace
}  // namespace rocketmq
