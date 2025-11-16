#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

#define private public
#define protected public
#include "consumer/RebalanceLitePullImpl.h"
#include "consumer/DefaultLitePullConsumerImpl.h"
#undef private
#undef protected

#include "consumer/DefaultLitePullConsumerConfigImpl.hpp"
#include "MessageQueueListener.h"
#include "OffsetStore.h"
#include "ProcessQueue.h"
#include "UtilAll.h"

namespace rocketmq {
namespace {

class RecordingOffsetStore : public OffsetStore {
 public:
  void load() override {}

  void updateOffset(const MQMessageQueue& mq, int64_t offset, bool increaseOnly) override {
    updates.emplace_back(mq, offset, increaseOnly);
  }

  int64_t readOffset(const MQMessageQueue& mq, ReadOffsetType) override {
    auto it = read_results.find(mq.toString());
    if (it != read_results.end()) {
      return it->second;
    }
    return default_read_offset;
  }

  void persist(const MQMessageQueue& mq) override { persisted.push_back(mq); }

  void persistAll(std::vector<MQMessageQueue>&) override {}

  void removeOffset(const MQMessageQueue& mq) override { removed.push_back(mq); }

  void setReadOffset(const MQMessageQueue& mq, int64_t offset) { read_results[mq.toString()] = offset; }

  int64_t default_read_offset = -1;
  std::map<std::string, int64_t> read_results;
  std::vector<MQMessageQueue> persisted;
  std::vector<MQMessageQueue> removed;
  std::vector<std::tuple<MQMessageQueue, int64_t, bool>> updates;
};

class CapturingMessageQueueListener : public MessageQueueListener {
 public:
  void messageQueueChanged(const std::string& topic,
                           std::vector<MQMessageQueue>& mqAll,
                           std::vector<MQMessageQueue>& mqDivided) override {
    if (should_throw) {
      throw std::runtime_error("listener failure");
    }
    last_topic = topic;
    all = mqAll;
    divided = mqDivided;
    callbacks++;
  }

  bool should_throw = false;
  int callbacks = 0;
  std::string last_topic;
  std::vector<MQMessageQueue> all;
  std::vector<MQMessageQueue> divided;
};

class StubLitePullConsumerImpl : public DefaultLitePullConsumerImpl {
 public:
  explicit StubLitePullConsumerImpl(DefaultLitePullConsumerConfigPtr config) : DefaultLitePullConsumerImpl(config) {}

  int64_t maxOffset(const MQMessageQueue& mq) override {
    last_max_offset_queue = mq;
    return max_offset_result;
  }

  int64_t searchOffset(const MQMessageQueue& mq, int64_t timestamp) override {
    last_search_offset_queue = mq;
    last_search_timestamp = timestamp;
    return search_offset_result;
  }

  int64_t max_offset_result = 0;
  int64_t search_offset_result = 0;
  MQMessageQueue last_max_offset_queue;
  MQMessageQueue last_search_offset_queue;
  int64_t last_search_timestamp = 0;
};

std::unique_ptr<StubLitePullConsumerImpl> makeLitePullConsumer() {
  auto config = std::make_shared<DefaultLitePullConsumerConfigImpl>();
  config->set_group_name("TestGroup");
  config->set_message_model(MessageModel::CLUSTERING);
  return std::unique_ptr<StubLitePullConsumerImpl>(new StubLitePullConsumerImpl(config));
}

}  // namespace

TEST(RebalanceLitePullImplTest, RemoveUnnecessaryMessageQueuePersistsAndRemovesOffsets) {
  auto consumer = makeLitePullConsumer();
  auto* store = new RecordingOffsetStore();
  consumer->offset_store_.reset(store);

  RebalanceLitePullImpl rebalance(consumer.get());

  MQMessageQueue mq("TopicA", "BrokerA", 0);
  rebalance.removeUnnecessaryMessageQueue(mq, std::make_shared<ProcessQueue>());

  ASSERT_EQ(1u, store->persisted.size());
  EXPECT_EQ(mq, store->persisted.front());
  ASSERT_EQ(1u, store->removed.size());
  EXPECT_EQ(mq, store->removed.front());
}

TEST(RebalanceLitePullImplTest, RemoveDirtyOffsetDelegatesToOffsetStore) {
  auto consumer = makeLitePullConsumer();
  auto* store = new RecordingOffsetStore();
  consumer->offset_store_.reset(store);

  RebalanceLitePullImpl rebalance(consumer.get());

  MQMessageQueue mq("TopicDirty", "BrokerB", 1);
  rebalance.removeDirtyOffset(mq);

  ASSERT_EQ(1u, store->removed.size());
  EXPECT_EQ(mq, store->removed.front());
}

TEST(RebalanceLitePullImplTest, ComputePullFromWhereUsesStoredOffsetWhenPresent) {
  auto consumer = makeLitePullConsumer();
  auto* store = new RecordingOffsetStore();
  consumer->offset_store_.reset(store);

  MQMessageQueue mq("TopicStored", "BrokerA", 2);
  store->setReadOffset(mq, 123);

  RebalanceLitePullImpl rebalance(consumer.get());
  auto offset = rebalance.computePullFromWhere(mq);

  EXPECT_EQ(123, offset);
}

TEST(RebalanceLitePullImplTest, ComputePullFromWhereFallsBackToMaxOffset) {
  auto consumer = makeLitePullConsumer();
  auto* store = new RecordingOffsetStore();
  store->default_read_offset = -1;
  consumer->offset_store_.reset(store);
  consumer->max_offset_result = 456;

  MQMessageQueue mq("NormalTopic", "BrokerA", 3);
  RebalanceLitePullImpl rebalance(consumer.get());

  auto offset = rebalance.computePullFromWhere(mq);

  EXPECT_EQ(456, offset);
  EXPECT_EQ(mq, consumer->last_max_offset_queue);
}

TEST(RebalanceLitePullImplTest, ComputePullFromWhereReturnsZeroForRetryTopic) {
  auto consumer = makeLitePullConsumer();
  auto* store = new RecordingOffsetStore();
  store->default_read_offset = -1;
  consumer->offset_store_.reset(store);

  std::string retryTopic = UtilAll::getRetryTopic(consumer->groupName());
  MQMessageQueue mq(retryTopic, "BrokerRetry", 0);

  RebalanceLitePullImpl rebalance(consumer.get());
  auto offset = rebalance.computePullFromWhere(mq);

  EXPECT_EQ(0, offset);
}

TEST(RebalanceLitePullImplTest, ComputePullFromWhereReadsFromTimestampWhenConfigured) {
  auto consumer = makeLitePullConsumer();
  consumer->getDefaultLitePullConsumerConfig()->set_consume_from_where(ConsumeFromWhere::CONSUME_FROM_TIMESTAMP);
  consumer->getDefaultLitePullConsumerConfig()->set_consume_timestamp("20200101010101");

  auto* store = new RecordingOffsetStore();
  store->default_read_offset = -1;
  consumer->offset_store_.reset(store);

  consumer->search_offset_result = 789;

  MQMessageQueue mq("TimedTopic", "BrokerT", 5);
  RebalanceLitePullImpl rebalance(consumer.get());

  auto offset = rebalance.computePullFromWhere(mq);

  EXPECT_EQ(789, offset);
  EXPECT_EQ(mq, consumer->last_search_offset_queue);
  EXPECT_EQ(20200101010101ULL, consumer->last_search_timestamp);
}

TEST(RebalanceLitePullImplTest, MessageQueueChangedForwardsToListener) {
  auto consumer = makeLitePullConsumer();
  auto listener = std::unique_ptr<CapturingMessageQueueListener>(new CapturingMessageQueueListener());
  auto* listener_ptr = listener.get();
  consumer->message_queue_listener_ = std::move(listener);

  RebalanceLitePullImpl rebalance(consumer.get());
  std::vector<MQMessageQueue> mq_all{MQMessageQueue("Topic", "Broker", 0)};
  std::vector<MQMessageQueue> mq_divided{MQMessageQueue("Topic", "Broker", 1)};

  rebalance.messageQueueChanged("Topic", mq_all, mq_divided);

  EXPECT_EQ(1, listener_ptr->callbacks);
  EXPECT_EQ("Topic", listener_ptr->last_topic);
  EXPECT_EQ(mq_all, listener_ptr->all);
  EXPECT_EQ(mq_divided, listener_ptr->divided);
}

TEST(RebalanceLitePullImplTest, MessageQueueChangedSwallowsListenerExceptions) {
  auto consumer = makeLitePullConsumer();
  auto listener = std::unique_ptr<CapturingMessageQueueListener>(new CapturingMessageQueueListener());
  listener->should_throw = true;
  consumer->message_queue_listener_ = std::move(listener);

  RebalanceLitePullImpl rebalance(consumer.get());
  std::vector<MQMessageQueue> mq_all;
  std::vector<MQMessageQueue> mq_divided;

  EXPECT_NO_THROW(rebalance.messageQueueChanged("Topic", mq_all, mq_divided));
}

}  // namespace rocketmq
