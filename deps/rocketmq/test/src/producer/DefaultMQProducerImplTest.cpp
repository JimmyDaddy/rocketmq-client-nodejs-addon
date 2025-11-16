#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "UtilAll.h"
#include "common/NamespaceUtil.h"
#include "MQMessageQueue.h"
#include "producer/DefaultMQProducerConfigImpl.hpp"

#define private public
#define protected public
#include "producer/DefaultMQProducerImpl.h"
#include "MQClientInstance.h"
#undef private
#undef protected

#include "producer/MQFaultStrategy.h"

namespace rocketmq {
namespace {

std::string uniqueSuffix(const std::string& prefix) {
  static std::atomic<int> counter{0};
  return prefix + std::to_string(counter.fetch_add(1) + 1);
}

std::shared_ptr<DefaultMQProducerConfigImpl> makeProducerConfig(const std::string& name_suffix) {
  auto config = std::make_shared<DefaultMQProducerConfigImpl>();
  config->set_group_name("UnitTestProducer" + name_suffix);
  config->set_instance_name("UnitTestInstance" + name_suffix);
  config->set_namesrv_addr("127.0.0.1:9876");
  return config;
}

MQClientInstancePtr makeClientInstance(const std::shared_ptr<DefaultMQProducerConfigImpl>& config) {
  auto client_id = config->buildMQClientId();
  return std::make_shared<MQClientInstance>(*config, client_id);
}

}  // namespace

TEST(DefaultMQProducerImplTest, StartAndShutdownRegisterProducerAndStopCleanly) {
  auto config = makeProducerConfig(uniqueSuffix("Start"));
  auto producer = DefaultMQProducerImpl::create(config);
  auto client_instance = makeClientInstance(config);
  producer->client_instance_ = client_instance;

  ASSERT_EQ(CREATE_JUST, producer->service_state_);

  ASSERT_NO_THROW(producer->start());
  EXPECT_EQ(RUNNING, producer->service_state_);
  EXPECT_TRUE(client_instance->isRunning());
  EXPECT_EQ(producer.get(), client_instance->selectProducer(config->group_name()));
  ASSERT_NE(nullptr, producer->async_send_executor_);

  producer->shutdown();
  EXPECT_EQ(SHUTDOWN_ALREADY, producer->service_state_);
  EXPECT_FALSE(client_instance->isRunning());
  EXPECT_EQ(nullptr, client_instance->selectProducer(config->group_name()));
}

TEST(DefaultMQProducerImplTest, SelectOneMessageQueueUsesFaultStrategy) {
  auto config = makeProducerConfig(uniqueSuffix("Fault"));
  auto producer = DefaultMQProducerImpl::create(config);

  auto mutable_info = std::make_shared<TopicPublishInfo>();
  mutable_info->message_queue_list_.push_back(MQMessageQueue("TopicSelect", "BrokerA", 0));
  TopicPublishInfoPtr info = mutable_info;

  const MQMessageQueue& expected = producer->mq_fault_strategy_->selectOneMessageQueue(info.get(), "BrokerB");
  const MQMessageQueue& selected = producer->selectOneMessageQueue(info.get(), "BrokerB");

  EXPECT_EQ(expected.topic(), selected.topic());
  EXPECT_EQ(expected.broker_name(), selected.broker_name());
  EXPECT_EQ(expected.queue_id(), selected.queue_id());
}

TEST(DefaultMQProducerImplTest, FetchPublishMessageQueuesReturnsNamespacedTopics) {
  auto config = makeProducerConfig(uniqueSuffix("Namespace"));
  config->set_name_space("TestNamespace");
  auto producer = DefaultMQProducerImpl::create(config);
  auto client_instance = makeClientInstance(config);
  producer->client_instance_ = client_instance;

  std::string namespaced_topic = NamespaceUtil::wrapNamespace(config->name_space(), "UserTopic");
  auto mutable_info = std::make_shared<TopicPublishInfo>();
  mutable_info->message_queue_list_.push_back(MQMessageQueue(namespaced_topic, "BrokerA", 0));
  mutable_info->message_queue_list_.push_back(MQMessageQueue(namespaced_topic, "BrokerA", 1));
  TopicPublishInfoPtr info = mutable_info;

  {
    std::lock_guard<std::mutex> guard(client_instance->topic_publish_info_table_mutex_);
    client_instance->topic_publish_info_table_[namespaced_topic] = info;
  }

  auto queues = producer->fetchPublishMessageQueues(namespaced_topic);
  ASSERT_EQ(mutable_info->message_queue_list_.size(), queues.size());
  for (size_t i = 0; i < queues.size(); ++i) {
    EXPECT_EQ(namespaced_topic, queues[i].topic());
    EXPECT_EQ(mutable_info->message_queue_list_[i].broker_name(), queues[i].broker_name());
    EXPECT_EQ(mutable_info->message_queue_list_[i].queue_id(), queues[i].queue_id());
  }
}

}  // namespace rocketmq
