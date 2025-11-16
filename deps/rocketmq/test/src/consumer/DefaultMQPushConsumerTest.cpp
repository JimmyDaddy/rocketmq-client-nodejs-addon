#include <gtest/gtest.h>

#include "DefaultMQPushConsumer.h"
#include "MQMessageExt.h"
#include "MQPushConsumer.h"
#include "common/UtilAll.h"

namespace rocketmq {

namespace {

class StubMQPushConsumer : public MQPushConsumer {
 public:
  virtual ~StubMQPushConsumer() = default;
  bool started{false};
  bool shutdown_called{false};
  bool suspended{false};
  bool resumed{false};
  std::vector<std::pair<std::string, std::string>> subscribe_calls;
  MessageListenerConcurrently* last_concurrent_listener{nullptr};
  MessageListenerOrderly* last_orderly_listener{nullptr};
  MQMessageListener* listener_to_return{nullptr};
  bool send_back_result{true};
  std::vector<std::pair<MessageExtPtr, int>> send_back_calls;
  std::vector<std::tuple<MessageExtPtr, int, std::string>> send_back_with_broker_calls;

  void start() override { started = true; }
  void shutdown() override { shutdown_called = true; }
  void suspend() override { suspended = true; }
  void resume() override { resumed = true; }

  MQMessageListener* getMessageListener() const override { return listener_to_return; }

  void registerMessageListener(MessageListenerConcurrently* messageListener) override {
    last_concurrent_listener = messageListener;
  }

  void registerMessageListener(MessageListenerOrderly* messageListener) override {
    last_orderly_listener = messageListener;
  }

  void subscribe(const std::string& topic, const std::string& subExpression) override {
    subscribe_calls.emplace_back(topic, subExpression);
  }

  bool sendMessageBack(MessageExtPtr msg, int delayLevel) override {
    send_back_calls.emplace_back(msg, delayLevel);
    return send_back_result;
  }

  bool sendMessageBack(MessageExtPtr msg, int delayLevel, const std::string& brokerName) override {
    send_back_with_broker_calls.emplace_back(msg, delayLevel, brokerName);
    return send_back_result;
  }
};

class DummyMessageListenerConcurrently : public MessageListenerConcurrently {
 public:
  ConsumeStatus consumeMessage(std::vector<MQMessageExt>&) override { return CONSUME_SUCCESS; }
};

class DummyMessageListenerOrderly : public MessageListenerOrderly {
 public:
  ConsumeStatus consumeMessage(std::vector<MQMessageExt>&) override { return CONSUME_SUCCESS; }
};

class TestableDefaultMQPushConsumer : public DefaultMQPushConsumer {
 public:
  explicit TestableDefaultMQPushConsumer(const std::string& group) : DefaultMQPushConsumer(group) {}

  void ReplaceImpl(std::shared_ptr<MQPushConsumer> impl) { push_consumer_impl_ = std::move(impl); }

  std::shared_ptr<MQPushConsumer> Impl() const { return push_consumer_impl_; }
};

}  // namespace

TEST(DefaultMQPushConsumerTest, AppliesDefaultGroupNameWhenEmpty) {
  TestableDefaultMQPushConsumer consumer("");
  EXPECT_EQ(DEFAULT_CONSUMER_GROUP, consumer.group_name());
}

TEST(DefaultMQPushConsumerTest, UsesProvidedGroupName) {
  TestableDefaultMQPushConsumer consumer("push-group");
  EXPECT_EQ("push-group", consumer.group_name());
}

TEST(DefaultMQPushConsumerTest, StartAndShutdownDelegateToImpl) {
  auto stub = std::make_shared<StubMQPushConsumer>();
  TestableDefaultMQPushConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  consumer.start();
  consumer.shutdown();

  EXPECT_TRUE(stub->started);
  EXPECT_TRUE(stub->shutdown_called);
}

TEST(DefaultMQPushConsumerTest, SuspendAndResumeDelegateToImpl) {
  auto stub = std::make_shared<StubMQPushConsumer>();
  TestableDefaultMQPushConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  consumer.suspend();
  consumer.resume();

  EXPECT_TRUE(stub->suspended);
  EXPECT_TRUE(stub->resumed);
}

TEST(DefaultMQPushConsumerTest, SubscribeForwardsTopicAndExpression) {
  auto stub = std::make_shared<StubMQPushConsumer>();
  TestableDefaultMQPushConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  consumer.subscribe("TopicTest", "tagA");
  ASSERT_EQ(1u, stub->subscribe_calls.size());
  EXPECT_EQ("TopicTest", stub->subscribe_calls[0].first);
  EXPECT_EQ("tagA", stub->subscribe_calls[0].second);
}

TEST(DefaultMQPushConsumerTest, RegisterMessageListenersDelegateToImpl) {
  auto stub = std::make_shared<StubMQPushConsumer>();
  TestableDefaultMQPushConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  DummyMessageListenerConcurrently concurrent_listener;
  DummyMessageListenerOrderly orderly_listener;

  consumer.registerMessageListener(&concurrent_listener);
  consumer.registerMessageListener(&orderly_listener);

  EXPECT_EQ(&concurrent_listener, stub->last_concurrent_listener);
  EXPECT_EQ(&orderly_listener, stub->last_orderly_listener);
}

TEST(DefaultMQPushConsumerTest, SendMessageBackDelegatesAndPropagatesResult) {
  auto stub = std::make_shared<StubMQPushConsumer>();
  stub->send_back_result = false;
  TestableDefaultMQPushConsumer consumer("group");
  consumer.ReplaceImpl(stub);

  auto message = std::make_shared<MQMessageExt>();
  EXPECT_FALSE(consumer.sendMessageBack(message, 3));
  EXPECT_FALSE(consumer.sendMessageBack(message, 7, "broker-a"));

  ASSERT_EQ(1u, stub->send_back_calls.size());
  EXPECT_EQ(3, stub->send_back_calls[0].second);
  ASSERT_EQ(1u, stub->send_back_with_broker_calls.size());
  EXPECT_EQ(7, std::get<1>(stub->send_back_with_broker_calls[0]));
  EXPECT_EQ("broker-a", std::get<2>(stub->send_back_with_broker_calls[0]));
}

}  // namespace rocketmq
