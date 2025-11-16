#include <gtest/gtest.h>

#define private public
#define protected public
#include "consumer/DefaultLitePullConsumerImpl.h"
#undef private
#undef protected

#include "consumer/AssignedMessageQueue.hpp"
#include "consumer/DefaultLitePullConsumerConfigImpl.hpp"
#include "MQMessageQueue.h"

namespace rocketmq {

namespace {

DefaultLitePullConsumerImplPtr makeConsumerImpl(MessageModel message_model = MessageModel::CLUSTERING) {
  auto config = std::make_shared<DefaultLitePullConsumerConfigImpl>();
  config->set_message_model(message_model);
  return DefaultLitePullConsumerImpl::create(config);
}

std::vector<MQMessageQueue> sortedQueues(std::vector<MQMessageQueue> queues) {
  std::sort(queues.begin(), queues.end());
  return queues;
}

}  // namespace

TEST(DefaultLitePullConsumerImplTest, IsSetEqualIgnoresOrderingDifferences) {
  auto consumer = makeConsumerImpl();

  std::vector<MQMessageQueue> current{MQMessageQueue("Topic", "BrokerA", 1), MQMessageQueue("Topic", "BrokerA", 2)};
  std::vector<MQMessageQueue> incoming{MQMessageQueue("Topic", "BrokerA", 2), MQMessageQueue("Topic", "BrokerA", 1)};

  EXPECT_TRUE(consumer->isSetEqual(incoming, current));

  incoming.emplace_back("Topic", "BrokerA", 3);
  EXPECT_FALSE(consumer->isSetEqual(incoming, current));
}

TEST(DefaultLitePullConsumerImplTest, MessageQueueListenerUsesDividedQueuesForClustering) {
  auto consumer = makeConsumerImpl(MessageModel::CLUSTERING);
  consumer->subscribe("TopicTest", "*");

  ASSERT_NE(nullptr, consumer->message_queue_listener_);

  std::vector<MQMessageQueue> mq_all{MQMessageQueue("TopicTest", "BrokerA", 0),
                                     MQMessageQueue("TopicTest", "BrokerB", 1)};
  std::vector<MQMessageQueue> mq_divided{MQMessageQueue("TopicTest", "BrokerB", 1)};

  consumer->message_queue_listener_->messageQueueChanged("TopicTest", mq_all, mq_divided);

  auto assigned = sortedQueues(consumer->assigned_message_queue_->messageQueues());
  EXPECT_EQ(sortedQueues(mq_divided), assigned);

  EXPECT_EQ(mq_divided.size(), consumer->task_table_.size());
}

TEST(DefaultLitePullConsumerImplTest, MessageQueueListenerUsesAllQueuesForBroadcasting) {
  auto consumer = makeConsumerImpl(MessageModel::BROADCASTING);
  consumer->subscribe("TopicBroadcast", "*");

  ASSERT_NE(nullptr, consumer->message_queue_listener_);

  std::vector<MQMessageQueue> mq_all{MQMessageQueue("TopicBroadcast", "BrokerA", 0),
                                     MQMessageQueue("TopicBroadcast", "BrokerA", 1)};
  std::vector<MQMessageQueue> mq_divided{MQMessageQueue("TopicBroadcast", "BrokerA", 0)};

  consumer->message_queue_listener_->messageQueueChanged("TopicBroadcast", mq_all, mq_divided);

  auto assigned = sortedQueues(consumer->assigned_message_queue_->messageQueues());
  EXPECT_EQ(sortedQueues(mq_all), assigned);
  EXPECT_EQ(mq_all.size(), consumer->task_table_.size());
}

}  // namespace rocketmq
