#include <gtest/gtest.h>

#include <vector>

#include "MQClientInstance.h"
#include "MQMessageQueue.h"
#include "common/PermName.h"
#include "producer/MQFaultStrategy.h"

using rocketmq::BrokerData;
using rocketmq::MQClientInstance;
using rocketmq::MQFaultStrategy;
using rocketmq::MQMessageQueue;
using rocketmq::PermName;
using rocketmq::TopicPublishInfoPtr;
using rocketmq::TopicRouteData;
using rocketmq::TopicRouteDataPtr;

namespace {

TopicPublishInfoPtr buildPublishInfo(const std::vector<std::pair<std::string, int>>& brokers) {
  auto route = std::make_shared<TopicRouteData>();
  for (const auto& entry : brokers) {
    const auto& name = entry.first;
    auto queues = entry.second;
    route->queue_datas().emplace_back(name, queues, queues, PermName::PERM_READ | PermName::PERM_WRITE);

    BrokerData broker(name);
    broker.broker_addrs()[rocketmq::MASTER_ID] = "127.0.0.1:10911";
    route->broker_datas().push_back(broker);
  }
  return MQClientInstance::topicRouteData2TopicPublishInfo("LatencyTestTopic", route);
}

}  // namespace

TEST(MQFaultStrategyTest, SkipsUnavailableBrokersWhenLatencyFaultEnabled) {
  auto info = buildPublishInfo({{"brokerA", 2}, {"brokerB", 2}});

  MQFaultStrategy strategy;
  strategy.setSendLatencyFaultEnable(true);
  strategy.updateFaultItem("brokerA", 5000 /* >= 3000ms bucket */, false);

  info->getSendWhichQueue().store(0);
  const MQMessageQueue& selected = strategy.selectOneMessageQueue(info.get(), "");
  EXPECT_EQ("brokerB", selected.broker_name());
}

TEST(MQFaultStrategyTest, IsolationOverridesLatencyThreshold) {
  auto info = buildPublishInfo({{"brokerA", 1}, {"brokerB", 1}});

  MQFaultStrategy strategy;
  strategy.setSendLatencyFaultEnable(true);

  info->getSendWhichQueue().store(0);
  strategy.updateFaultItem("brokerA", 10 /* below thresholds */, false);
  const MQMessageQueue& healthy = strategy.selectOneMessageQueue(info.get(), "");
  EXPECT_EQ("brokerA", healthy.broker_name());

  info->getSendWhichQueue().store(0);
  strategy.updateFaultItem("brokerA", 10 /* same latency */, true /* isolation forces penalty */);
  const MQMessageQueue& isolated = strategy.selectOneMessageQueue(info.get(), "");
  EXPECT_EQ("brokerB", isolated.broker_name());
}

TEST(MQFaultStrategyTest, FallsBackToLeastFaultyBrokerWhenAllUnavailable) {
  auto info = buildPublishInfo({{"brokerA", 2}, {"brokerB", 2}});

  MQFaultStrategy strategy;
  strategy.setSendLatencyFaultEnable(true);
  strategy.updateFaultItem("brokerA", 5000, false);
  strategy.updateFaultItem("brokerB", 600, false);  // still marked unavailable but lower latency

  info->getSendWhichQueue().store(0);
  const MQMessageQueue& fallback = strategy.selectOneMessageQueue(info.get(), "");
  EXPECT_EQ("brokerB", fallback.broker_name());
  EXPECT_LT(fallback.queue_id(), 2);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQFaultStrategyTest.*";
  return RUN_ALL_TESTS();
}
