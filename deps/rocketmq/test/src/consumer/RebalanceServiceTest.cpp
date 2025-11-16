#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <string>

#include "MQClientInstance.h"
#include "MQClientConfigImpl.hpp"
#include "consumer/MQConsumerInner.h"
#include "consumer/RebalanceService.h"

using rocketmq::ConsumeFromWhere;
using rocketmq::ConsumeType;
using rocketmq::MessageModel;
using rocketmq::MQClientConfigImpl;
using rocketmq::MQClientInstance;
using rocketmq::SubscriptionData;

namespace {

class RecordingConsumer : public rocketmq::MQConsumerInner {
 public:
  explicit RecordingConsumer(std::string group)
      : group_(std::move(group)), call_count_(0), message_model_(MessageModel::CLUSTERING) {}

  const std::string& groupName() const override { return group_; }
  MessageModel messageModel() const override { return message_model_; }
  ConsumeType consumeType() const override { return ConsumeType::CONSUME_ACTIVELY; }
  ConsumeFromWhere consumeFromWhere() const override { return ConsumeFromWhere::CONSUME_FROM_LAST_OFFSET; }

  std::vector<SubscriptionData> subscriptions() const override { return {}; }

  void updateTopicSubscribeInfo(const std::string&, std::vector<rocketmq::MQMessageQueue>&) override {}

  void doRebalance() override {
    std::lock_guard<std::mutex> lock(mutex_);
    ++call_count_;
    cv_.notify_all();
  }

  void persistConsumerOffset() override {}

  rocketmq::ConsumerRunningInfo* consumerRunningInfo() override { return nullptr; }

  bool WaitForRebalance(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, timeout, [&] { return call_count_ > 0; });
  }

 private:
  std::string group_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  int call_count_;
  MessageModel message_model_;
};

std::shared_ptr<MQClientInstance> MakeClientInstance(const std::string& prefix) {
  auto config = std::make_shared<MQClientConfigImpl>();
  config->set_namesrv_addr("127.0.0.1:9876");
  config->set_instance_name(prefix + "Instance");
  auto client_id = config->buildMQClientId();
  return std::make_shared<MQClientInstance>(*config, client_id);
}

}  // namespace

TEST(RebalanceServiceTest, WakesUpAndInvokesClientRebalance) {
  auto client_instance = MakeClientInstance("RebalanceServiceTest");
  RecordingConsumer consumer("RebalanceServiceGroup");
  ASSERT_TRUE(client_instance->registerConsumer(consumer.groupName(), &consumer));

  rocketmq::RebalanceService service(client_instance.get());
  service.start();

  service.wakeup();

  EXPECT_TRUE(consumer.WaitForRebalance(std::chrono::milliseconds(500)));

  service.shutdown();
  client_instance->unregisterConsumer(consumer.groupName());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "RebalanceServiceTest.*";
  return RUN_ALL_TESTS();
}
