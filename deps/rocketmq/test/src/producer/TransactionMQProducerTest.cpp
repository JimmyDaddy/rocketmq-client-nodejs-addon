#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "MQMessage.h"
#include "MQMessageExt.h"
#include "TransactionListener.h"
#include "TransactionMQProducer.h"

using rocketmq::LocalTransactionState;
using rocketmq::MQMessage;
using rocketmq::MQMessageQueue;
using rocketmq::MessageQueueSelector;
using rocketmq::RequestCallback;
using rocketmq::SendCallback;
using rocketmq::SendResult;
using rocketmq::TransactionListener;
using rocketmq::TransactionMQProducer;
using rocketmq::TransactionSendResult;

namespace {

class DummyTransactionListener : public TransactionListener {
 public:
  LocalTransactionState executeLocalTransaction(const MQMessage&, void*) override {
    return LocalTransactionState::COMMIT_MESSAGE;
  }

  LocalTransactionState checkLocalTransaction(const rocketmq::MQMessageExt&) override {
    return LocalTransactionState::UNKNOWN;
  }
};

class RecordingMQProducer : public rocketmq::MQProducer {
 public:
  void start() override {}
  void shutdown() override {}

  std::vector<MQMessageQueue> fetchPublishMessageQueues(const std::string&) override { return {}; }

  SendResult send(MQMessage&) override { return SendResult(); }
  SendResult send(MQMessage&, long) override { return SendResult(); }
  SendResult send(MQMessage&, const MQMessageQueue&) override { return SendResult(); }
  SendResult send(MQMessage&, const MQMessageQueue&, long) override { return SendResult(); }

  void send(MQMessage&, SendCallback*, long) noexcept override {}
  void send(MQMessage&, SendCallback*) noexcept override {}
  void send(MQMessage&, const MQMessageQueue&, SendCallback*) noexcept override {}
  void send(MQMessage&, const MQMessageQueue&, SendCallback*, long) noexcept override {}

  void sendOneway(MQMessage&) override {}
  void sendOneway(MQMessage&, const MQMessageQueue&) override {}

  SendResult send(MQMessage&, MessageQueueSelector*, void*) override { return SendResult(); }
  SendResult send(MQMessage&, MessageQueueSelector*, void*, long) override { return SendResult(); }
  void send(MQMessage&, MessageQueueSelector*, void*, SendCallback*) noexcept override {}
  void send(MQMessage&, MessageQueueSelector*, void*, SendCallback*, long) noexcept override {}
  void sendOneway(MQMessage&, MessageQueueSelector*, void*) override {}

  TransactionSendResult sendMessageInTransaction(MQMessage& msg, void* arg) override {
    ++call_count_;
    last_arg_ = arg;
    last_topic_ = msg.topic();
    TransactionSendResult result(SendResult{});
    result.set_local_transaction_state(LocalTransactionState::COMMIT_MESSAGE);
    return result;
  }

  SendResult send(std::vector<MQMessage>&) override { return SendResult(); }
  SendResult send(std::vector<MQMessage>&, long) override { return SendResult(); }
  SendResult send(std::vector<MQMessage>&, const MQMessageQueue&) override { return SendResult(); }
  SendResult send(std::vector<MQMessage>&, const MQMessageQueue&, long) override { return SendResult(); }

  void send(std::vector<MQMessage>&, SendCallback*) override {}
  void send(std::vector<MQMessage>&, SendCallback*, long) override {}
  void send(std::vector<MQMessage>&, const MQMessageQueue&, SendCallback*) override {}
  void send(std::vector<MQMessage>&, const MQMessageQueue&, SendCallback*, long) override {}

  MQMessage request(MQMessage&, long) override { return MQMessage(); }
  void request(MQMessage&, RequestCallback*, long) override {}
  MQMessage request(MQMessage&, const MQMessageQueue&, long) override { return MQMessage(); }
  void request(MQMessage&, const MQMessageQueue&, RequestCallback*, long) override {}
  MQMessage request(MQMessage&, MessageQueueSelector*, void*, long) override { return MQMessage(); }
  void request(MQMessage&, MessageQueueSelector*, void*, RequestCallback*, long) override {}

  int call_count() const { return call_count_; }
  void* last_arg() const { return last_arg_; }
  const std::string& last_topic() const { return last_topic_; }

 private:
  int call_count_ = 0;
  void* last_arg_ = nullptr;
  std::string last_topic_;
};

class TestableTransactionMQProducer : public TransactionMQProducer {
 public:
  explicit TestableTransactionMQProducer(const std::string& group) : TransactionMQProducer(group) {}

  void InjectProducerImpl(const std::shared_ptr<rocketmq::MQProducer>& impl) { producer_impl_ = impl; }
};

}  // namespace

TEST(TransactionMQProducerTest, StoresTransactionListenerInConfig) {
  TransactionMQProducer producer("groupA");
  DummyTransactionListener listener;

  EXPECT_EQ(nullptr, producer.getTransactionListener());
  producer.setTransactionListener(&listener);
  EXPECT_EQ(&listener, producer.getTransactionListener());
}

TEST(TransactionMQProducerTest, DelegatesTransactionalSendToInjectedImpl) {
  TestableTransactionMQProducer producer("groupB");
  auto recording_impl = std::make_shared<RecordingMQProducer>();
  producer.InjectProducerImpl(recording_impl);

  MQMessage message("TestTopic", "hello world");
  void* arg = reinterpret_cast<void*>(0x1234);

  auto result = producer.sendMessageInTransaction(message, arg);

  EXPECT_EQ(1, recording_impl->call_count());
  EXPECT_EQ("TestTopic", recording_impl->last_topic());
  EXPECT_EQ(arg, recording_impl->last_arg());
  EXPECT_EQ(LocalTransactionState::COMMIT_MESSAGE, result.local_transaction_state());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "TransactionMQProducerTest.*";
  return RUN_ALL_TESTS();
}
