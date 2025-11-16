#include <gtest/gtest.h>

#include "DefaultLitePullConsumer.h"
#include "LitePullConsumer.h"
#include "MQMessageQueue.h"
#include "TopicMessageQueueChangeListener.h"
#include "common/UtilAll.h"

namespace rocketmq {

namespace {

class StubLitePullConsumer : public LitePullConsumer {
 public:
  bool started{false};
  bool shutdown_called{false};
  bool auto_commit{true};
  std::vector<std::pair<std::string, std::string>> subscribe_calls;
  std::vector<std::string> unsubscribe_topics;
  std::vector<MQMessageExt> poll_response;
  std::vector<MQMessageExt> timed_poll_response;
  long last_poll_timeout{0};
  std::string last_fetch_topic;
  std::vector<MQMessageQueue> fetch_result;
  std::vector<std::vector<MQMessageQueue>> assign_calls;
  MQMessageQueue seek_queue;
  int64_t seek_offset{-1};
  MQMessageQueue seek_begin_queue;
  MQMessageQueue seek_end_queue;
  MQMessageQueue offset_for_timestamp_queue;
  int64_t offset_for_timestamp_timestamp{-1};
  int64_t offset_for_timestamp_result{0};
  std::vector<std::vector<MQMessageQueue>> paused_sets;
  std::vector<std::vector<MQMessageQueue>> resumed_sets;
  bool commit_called{false};
  MQMessageQueue committed_queue;
  int64_t committed_value{-1};
  std::string last_listener_topic;
  TopicMessageQueueChangeListener* last_listener{nullptr};

  void start() override { started = true; }
  void shutdown() override { shutdown_called = true; }

  bool isAutoCommit() const override { return auto_commit; }
  void setAutoCommit(bool auto_commit_flag) override { auto_commit = auto_commit_flag; }

  void subscribe(const std::string& topic, const std::string& subExpression) override {
    subscribe_calls.emplace_back(topic, subExpression);
  }

  void subscribe(const std::string& topic, const MessageSelector& selector) override {
    subscribe_calls.emplace_back(topic, selector.expression());
  }

  void unsubscribe(const std::string& topic) override { unsubscribe_topics.push_back(topic); }

  std::vector<MQMessageExt> poll() override { return poll_response; }

  std::vector<MQMessageExt> poll(long timeout) override {
    last_poll_timeout = timeout;
    return timed_poll_response;
  }

  std::vector<MQMessageQueue> fetchMessageQueues(const std::string& topic) override {
    last_fetch_topic = topic;
    return fetch_result;
  }

  void assign(const std::vector<MQMessageQueue>& messageQueues) override { assign_calls.push_back(messageQueues); }

  void seek(const MQMessageQueue& messageQueue, int64_t offset) override {
    seek_queue = messageQueue;
    seek_offset = offset;
  }

  void seekToBegin(const MQMessageQueue& messageQueue) override { seek_begin_queue = messageQueue; }

  void seekToEnd(const MQMessageQueue& messageQueue) override { seek_end_queue = messageQueue; }

  int64_t offsetForTimestamp(const MQMessageQueue& messageQueue, int64_t timestamp) override {
    offset_for_timestamp_queue = messageQueue;
    offset_for_timestamp_timestamp = timestamp;
    return offset_for_timestamp_result;
  }

  void pause(const std::vector<MQMessageQueue>& messageQueues) override { paused_sets.push_back(messageQueues); }

  void resume(const std::vector<MQMessageQueue>& messageQueues) override { resumed_sets.push_back(messageQueues); }

  void commitSync() override { commit_called = true; }

  int64_t committed(const MQMessageQueue& messageQueue) override {
    committed_queue = messageQueue;
    return committed_value;
  }

  void registerTopicMessageQueueChangeListener(
      const std::string& topic,
      TopicMessageQueueChangeListener* topicMessageQueueChangeListener) override {
    last_listener_topic = topic;
    last_listener = topicMessageQueueChangeListener;
  }

  std::unique_ptr<PullResult> pullOnce(const MQMessageQueue&,
                                       const std::string&,
                                       int64_t,
                                       int,
                                       bool,
                                       long) override {
    return nullptr;
  }
};

class DummyQueueChangeListener : public TopicMessageQueueChangeListener {
 public:
  void onChanged(const std::string&, const std::vector<MQMessageQueue>&) override {}
};

class TestableDefaultLitePullConsumer : public DefaultLitePullConsumer {
 public:
  explicit TestableDefaultLitePullConsumer(const std::string& group) : DefaultLitePullConsumer(group) {}

  void ReplaceImpl(std::shared_ptr<LitePullConsumer> impl) { pull_consumer_impl_ = std::move(impl); }

  std::shared_ptr<LitePullConsumer> Impl() const { return pull_consumer_impl_; }
};

MQMessageQueue makeQueue(const std::string& topic, const std::string& broker, int queueId) {
  return MQMessageQueue(topic, broker, queueId);
}

}  // namespace

TEST(DefaultLitePullConsumerTest, AppliesDefaultGroupNameWhenEmpty) {
  TestableDefaultLitePullConsumer consumer("");
  EXPECT_EQ(DEFAULT_CONSUMER_GROUP, consumer.group_name());
}

TEST(DefaultLitePullConsumerTest, UsesProvidedGroupName) {
  const std::string group = "test-group";
  TestableDefaultLitePullConsumer consumer(group);
  EXPECT_EQ(group, consumer.group_name());
}

TEST(DefaultLitePullConsumerTest, AutoCommitDelegatesToImpl) {
  auto stub = std::make_shared<StubLitePullConsumer>();
  TestableDefaultLitePullConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  EXPECT_TRUE(consumer.isAutoCommit());
  consumer.setAutoCommit(false);
  EXPECT_FALSE(stub->auto_commit);
  EXPECT_FALSE(consumer.isAutoCommit());

  consumer.setAutoCommit(true);
  EXPECT_TRUE(stub->auto_commit);
  EXPECT_TRUE(consumer.isAutoCommit());
}

TEST(DefaultLitePullConsumerTest, SubscribeForwardsTopicAndExpression) {
  auto stub = std::make_shared<StubLitePullConsumer>();
  TestableDefaultLitePullConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  consumer.subscribe("TopicTest", "tagA || tagB");
  ASSERT_EQ(1u, stub->subscribe_calls.size());
  EXPECT_EQ("TopicTest", stub->subscribe_calls[0].first);
  EXPECT_EQ("tagA || tagB", stub->subscribe_calls[0].second);
}

TEST(DefaultLitePullConsumerTest, AssignForwardsQueueListVerbatim) {
  auto stub = std::make_shared<StubLitePullConsumer>();
  TestableDefaultLitePullConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  std::vector<MQMessageQueue> queues{makeQueue("TopicA", "BrokerA", 0), makeQueue("TopicA", "BrokerA", 1)};
  consumer.assign(queues);

  ASSERT_EQ(1u, stub->assign_calls.size());
  EXPECT_EQ(queues, stub->assign_calls[0]);
}

TEST(DefaultLitePullConsumerTest, SeekAndCommittedDelegateToImpl) {
  auto stub = std::make_shared<StubLitePullConsumer>();
  stub->committed_value = 4096;
  TestableDefaultLitePullConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  MQMessageQueue queue = makeQueue("TopicOff", "BrokerX", 3);
  consumer.seek(queue, 1234);
  EXPECT_EQ(queue, stub->seek_queue);
  EXPECT_EQ(1234, stub->seek_offset);

  EXPECT_EQ(4096, consumer.committed(queue));
  EXPECT_EQ(queue, stub->committed_queue);
}

TEST(DefaultLitePullConsumerTest, PauseAndResumePropagateToImpl) {
  auto stub = std::make_shared<StubLitePullConsumer>();
  TestableDefaultLitePullConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  std::vector<MQMessageQueue> queues{makeQueue("TopicB", "BrokerB", 5)};
  consumer.pause(queues);
  consumer.resume(queues);

  ASSERT_EQ(1u, stub->paused_sets.size());
  EXPECT_EQ(queues, stub->paused_sets[0]);
  ASSERT_EQ(1u, stub->resumed_sets.size());
  EXPECT_EQ(queues, stub->resumed_sets[0]);
}

TEST(DefaultLitePullConsumerTest, RegisterTopicMessageQueueChangeListenerForwardsPointers) {
  auto stub = std::make_shared<StubLitePullConsumer>();
  TestableDefaultLitePullConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  DummyQueueChangeListener listener;
  consumer.registerTopicMessageQueueChangeListener("TopicC", &listener);

  EXPECT_EQ("TopicC", stub->last_listener_topic);
  EXPECT_EQ(&listener, stub->last_listener);
}

TEST(DefaultLitePullConsumerTest, PollVariantsReturnImplValues) {
  auto stub = std::make_shared<StubLitePullConsumer>();
  MQMessageExt msg1;
  msg1.set_topic("TopicPoll");
  MQMessageExt msg2;
  msg2.set_topic("TopicPollDelayed");
  stub->poll_response = {msg1};
  stub->timed_poll_response = {msg2};

  TestableDefaultLitePullConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  auto immediate = consumer.poll();
  ASSERT_EQ(1u, immediate.size());
  EXPECT_EQ("TopicPoll", immediate[0].topic());

  auto delayed = consumer.poll(2500);
  ASSERT_EQ(1u, delayed.size());
  EXPECT_EQ("TopicPollDelayed", delayed[0].topic());
  EXPECT_EQ(2500, stub->last_poll_timeout);
}

TEST(DefaultLitePullConsumerTest, CommitSyncCallsUnderlyingImplementation) {
  auto stub = std::make_shared<StubLitePullConsumer>();
  TestableDefaultLitePullConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  consumer.commitSync();
  EXPECT_TRUE(stub->commit_called);
}

}  // namespace rocketmq
