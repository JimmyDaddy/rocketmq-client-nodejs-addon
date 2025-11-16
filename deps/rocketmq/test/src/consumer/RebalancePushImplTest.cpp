#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#define private public
#define protected public
#include "consumer/RebalancePushImpl.h"
#include "consumer/DefaultMQPushConsumerImpl.h"
#undef private
#undef protected

#include "MQMessageQueue.h"
#include "ProcessQueue.h"
#include "PullRequest.h"
#include "common/UtilAll.h"
#include "consumer/DefaultMQPushConsumerConfigImpl.hpp"
#include "OffsetStore.h"

namespace rocketmq {
namespace {

class RecordingOffsetStore : public OffsetStore {
 public:
  void load() override {}

  void updateOffset(const MQMessageQueue& mq, int64_t offset, bool increaseOnly) override {
    updates.emplace_back(mq, offset, increaseOnly);
  }

  int64_t readOffset(const MQMessageQueue& mq, ReadOffsetType) override {
    auto it = read_offsets.find(mq.toString());
    if (it != read_offsets.end()) {
      return it->second;
    }
    return default_read_offset;
  }

  void persist(const MQMessageQueue& mq) override { persisted.push_back(mq); }

  void persistAll(std::vector<MQMessageQueue>&) override {}

  void removeOffset(const MQMessageQueue& mq) override { removed.push_back(mq); }

  void setReadOffset(const MQMessageQueue& mq, int64_t offset) { read_offsets[mq.toString()] = offset; }

  int64_t default_read_offset = -1;
  std::map<std::string, int64_t> read_offsets;
  std::vector<MQMessageQueue> persisted;
  std::vector<MQMessageQueue> removed;
  std::vector<std::tuple<MQMessageQueue, int64_t, bool>> updates;
};

class DummyConcurrentListener : public MessageListenerConcurrently {
 public:
  ConsumeStatus consumeMessage(std::vector<MQMessageExt>&) override { return CONSUME_SUCCESS; }
};

class DummyOrderlyListener : public MessageListenerOrderly {
 public:
  ConsumeStatus consumeMessage(std::vector<MQMessageExt>&) override { return CONSUME_SUCCESS; }
};

class RecordingPushConsumer : public DefaultMQPushConsumerImpl {
 public:
  explicit RecordingPushConsumer(DefaultMQPushConsumerConfigPtr config) : DefaultMQPushConsumerImpl(config) {}

  void executePullRequestImmediately(PullRequestPtr pullRequest) override {
    dispatched_requests.push_back(pullRequest);
  }

  int64_t maxOffset(const MQMessageQueue& mq) override {
    last_max_offset_queue = mq;
    return max_offset_result;
  }

  int64_t searchOffset(const MQMessageQueue& mq, int64_t timestamp) override {
    last_search_offset_queue = mq;
    last_search_timestamp = timestamp;
    return search_offset_result;
  }

  std::vector<PullRequestPtr> dispatched_requests;
  MQMessageQueue last_max_offset_queue;
  MQMessageQueue last_search_offset_queue;
  int64_t last_search_timestamp = 0;
  int64_t max_offset_result = 0;
  int64_t search_offset_result = 0;
};

using RecordingPushConsumerPtr = std::shared_ptr<RecordingPushConsumer>;

RecordingPushConsumerPtr makeConsumer(MessageModel model = MessageModel::CLUSTERING) {
  auto config = std::make_shared<DefaultMQPushConsumerConfigImpl>();
  config->set_group_name("GID_TestGroup");
  config->set_message_model(model);
  return RecordingPushConsumerPtr(new RecordingPushConsumer(config));
}

}  // namespace

TEST(RebalancePushImplTest, RemoveUnnecessaryMessageQueuePersistsOffsetsForConcurrentConsumers) {
  auto consumer = makeConsumer();
  auto* store = new RecordingOffsetStore();
  consumer->offset_store_.reset(store);
  auto listener_holder = std::unique_ptr<DummyConcurrentListener>(new DummyConcurrentListener());
  consumer->message_listener_ = listener_holder.get();

  RebalancePushImpl rebalance(consumer.get());
  MQMessageQueue mq("TopicA", "BrokerA", 0);
  auto pq = std::make_shared<ProcessQueue>();

  EXPECT_TRUE(rebalance.removeUnnecessaryMessageQueue(mq, pq));
  ASSERT_EQ(1u, store->persisted.size());
  EXPECT_EQ(mq, store->persisted.front());
  ASSERT_EQ(1u, store->removed.size());
  EXPECT_EQ(mq, store->removed.front());
}

TEST(RebalancePushImplTest, RemoveUnnecessaryMessageQueueReturnsFalseWhenLockUnavailable) {
  auto consumer = makeConsumer();
  auto* store = new RecordingOffsetStore();
  consumer->offset_store_.reset(store);
  auto listener_holder = std::unique_ptr<DummyOrderlyListener>(new DummyOrderlyListener());
  consumer->message_listener_ = listener_holder.get();

  RebalancePushImpl rebalance(consumer.get());
  MQMessageQueue mq("TopicB", "BrokerB", 1);
  auto pq = std::make_shared<ProcessQueue>();

  pq->lock_consume().lock();
  EXPECT_FALSE(rebalance.removeUnnecessaryMessageQueue(mq, pq));
  EXPECT_EQ(1, pq->try_unlock_times());
  pq->lock_consume().unlock();
}

TEST(RebalancePushImplTest, RemoveDirtyOffsetDelegatesToOffsetStore) {
  auto consumer = makeConsumer();
  auto* store = new RecordingOffsetStore();
  consumer->offset_store_.reset(store);
  RebalancePushImpl rebalance(consumer.get());

  MQMessageQueue mq("TopicC", "BrokerC", 2);
  rebalance.removeDirtyOffset(mq);

  ASSERT_EQ(1u, store->removed.size());
  EXPECT_EQ(mq, store->removed.front());
}

TEST(RebalancePushImplTest, ComputePullFromWhereReturnsStoredOffset) {
  auto consumer = makeConsumer();
  auto* store = new RecordingOffsetStore();
  store->setReadOffset(MQMessageQueue("TopicStored", "Broker", 0), 321);
  consumer->offset_store_.reset(store);

  RebalancePushImpl rebalance(consumer.get());
  MQMessageQueue mq("TopicStored", "Broker", 0);

  EXPECT_EQ(321, rebalance.computePullFromWhere(mq));
}

TEST(RebalancePushImplTest, ComputePullFromWhereFallsBackToMaxOffsetForLastOffset) {
  auto consumer = makeConsumer();
  auto* store = new RecordingOffsetStore();
  store->default_read_offset = -1;
  consumer->offset_store_.reset(store);
  consumer->max_offset_result = 555;

  RebalancePushImpl rebalance(consumer.get());
  MQMessageQueue mq("TopicNormal", "Broker", 3);

  EXPECT_EQ(555, rebalance.computePullFromWhere(mq));
  EXPECT_EQ(mq, consumer->last_max_offset_queue);
}

TEST(RebalancePushImplTest, ComputePullFromWhereReturnsZeroForRetryTopic) {
  auto consumer = makeConsumer();
  auto* store = new RecordingOffsetStore();
  store->default_read_offset = -1;
  consumer->offset_store_.reset(store);

  std::string retryTopic = UtilAll::getRetryTopic(consumer->groupName());
  MQMessageQueue mq(retryTopic, "BrokerRetry", 0);

  RebalancePushImpl rebalance(consumer.get());
  EXPECT_EQ(0, rebalance.computePullFromWhere(mq));
}

TEST(RebalancePushImplTest, ComputePullFromWhereFirstOffsetDefaultsToZero) {
  auto consumer = makeConsumer();
  consumer->getDefaultMQPushConsumerConfig()->set_consume_from_where(CONSUME_FROM_FIRST_OFFSET);
  auto* store = new RecordingOffsetStore();
  store->default_read_offset = -1;
  consumer->offset_store_.reset(store);

  RebalancePushImpl rebalance(consumer.get());
  MQMessageQueue mq("TopicFirst", "Broker", 4);

  EXPECT_EQ(0, rebalance.computePullFromWhere(mq));
}

TEST(RebalancePushImplTest, ComputePullFromWhereTimestampUsesSearchOffset) {
  auto consumer = makeConsumer();
  consumer->getDefaultMQPushConsumerConfig()->set_consume_from_where(CONSUME_FROM_TIMESTAMP);
  consumer->getDefaultMQPushConsumerConfig()->set_consume_timestamp("20220101010101");
  auto* store = new RecordingOffsetStore();
  store->default_read_offset = -1;
  consumer->offset_store_.reset(store);
  consumer->search_offset_result = 890;

  RebalancePushImpl rebalance(consumer.get());
  MQMessageQueue mq("TopicTimed", "Broker", 5);

  EXPECT_EQ(890, rebalance.computePullFromWhere(mq));
  EXPECT_EQ(mq, consumer->last_search_offset_queue);
  EXPECT_EQ(20220101010101LL, consumer->last_search_timestamp);
}

TEST(RebalancePushImplTest, ComputePullFromWhereTimestampRetryTopicUsesMaxOffset) {
  auto consumer = makeConsumer();
  consumer->getDefaultMQPushConsumerConfig()->set_consume_from_where(CONSUME_FROM_TIMESTAMP);
  auto* store = new RecordingOffsetStore();
  store->default_read_offset = -1;
  consumer->offset_store_.reset(store);
  consumer->max_offset_result = 77;

  std::string retryTopic = UtilAll::getRetryTopic(consumer->groupName());
  MQMessageQueue mq(retryTopic, "BrokerRetry", 1);

  RebalancePushImpl rebalance(consumer.get());
  EXPECT_EQ(77, rebalance.computePullFromWhere(mq));
  EXPECT_EQ(mq, consumer->last_max_offset_queue);
}

TEST(RebalancePushImplTest, DispatchPullRequestExecutesAllRequests) {
  auto consumer = makeConsumer();
  auto* store = new RecordingOffsetStore();
  consumer->offset_store_.reset(store);
  auto listener_holder = std::unique_ptr<DummyConcurrentListener>(new DummyConcurrentListener());
  consumer->message_listener_ = listener_holder.get();

  RebalancePushImpl rebalance(consumer.get());
  auto requestA = std::make_shared<PullRequest>();
  requestA->set_consumer_group(consumer->groupName());
  auto requestB = std::make_shared<PullRequest>();
  requestB->set_consumer_group(consumer->groupName());

  std::vector<PullRequestPtr> requests{requestA, requestB};
  rebalance.dispatchPullRequest(requests);

  ASSERT_EQ(2u, consumer->dispatched_requests.size());
  EXPECT_EQ(requestA, consumer->dispatched_requests[0]);
  EXPECT_EQ(requestB, consumer->dispatched_requests[1]);
}

}  // namespace rocketmq
