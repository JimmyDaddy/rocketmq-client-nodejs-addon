#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "MQClientInstance.h"
#include "common/PermName.h"

using rocketmq::BrokerData;
using rocketmq::MQClientInstance;
using rocketmq::TopicPublishInfoPtr;
using rocketmq::TopicRouteDataPtr;

namespace {

TopicRouteDataPtr BuildRoute(const std::vector<std::tuple<std::string, int, int, int>>& specs) {
  auto route = std::make_shared<rocketmq::TopicRouteData>();
  std::set<std::string> seen;
  int port = 11000;
  for (const auto& spec : specs) {
    const std::string& broker = std::get<0>(spec);
    auto read = std::get<1>(spec);
    auto write = std::get<2>(spec);
    auto perm = std::get<3>(spec);
    route->queue_datas().emplace_back(broker, read, write, perm);
    if (seen.insert(broker).second) {
      BrokerData data(broker);
      data.broker_addrs()[rocketmq::MASTER_ID] = "127.0.0.1:" + std::to_string(port++);
      route->broker_datas().push_back(data);
    }
  }
  return route;
}

}  // namespace

TEST(TopicPublishInfoTest, SelectOneMessageQueueAvoidsLastBrokerWhenPossible) {
  auto perms = rocketmq::PermName::PERM_READ | rocketmq::PermName::PERM_WRITE;
  auto route = BuildRoute({{"brokerA", 2, 2, perms}, {"brokerB", 2, 2, perms}});

  TopicPublishInfoPtr info = MQClientInstance::topicRouteData2TopicPublishInfo("TestTopic", route);
  ASSERT_TRUE(info->ok());

  auto mq = info->selectOneMessageQueue("brokerA");
  EXPECT_EQ("brokerB", mq.broker_name());

  mq = info->selectOneMessageQueue("brokerB");
  EXPECT_EQ("brokerA", mq.broker_name());
}

TEST(TopicPublishInfoTest, SelectOneMessageQueueFallsBackWhenSingleBroker) {
  auto perms = rocketmq::PermName::PERM_READ | rocketmq::PermName::PERM_WRITE;
  auto route = BuildRoute({{"solo", 1, 1, perms}});
  TopicPublishInfoPtr info = MQClientInstance::topicRouteData2TopicPublishInfo("SoloTopic", route);

  auto mq = info->selectOneMessageQueue("solo");
  EXPECT_EQ("solo", mq.broker_name());
  EXPECT_EQ(0, mq.queue_id());
}

TEST(TopicPublishInfoTest, SelectOneMessageQueueRoundRobinAcrossQueues) {
  auto perms = rocketmq::PermName::PERM_READ | rocketmq::PermName::PERM_WRITE;
  auto route = BuildRoute({{"brokerA", 1, 3, perms}});
  TopicPublishInfoPtr info = MQClientInstance::topicRouteData2TopicPublishInfo("RRTopic", route);

  std::vector<int> ids;
  for (int i = 0; i < 4; ++i) {
    ids.push_back(info->selectOneMessageQueue().queue_id());
  }
  ASSERT_EQ(std::vector<int>({0, 1, 2, 0}), ids);
}

TEST(TopicPublishInfoTest, GetQueueIdByBrokerMatchesRouteData) {
  auto perms = rocketmq::PermName::PERM_READ | rocketmq::PermName::PERM_WRITE;
  auto route = BuildRoute({{"brokerA", 2, 4, perms}, {"brokerB", 2, 1, perms}});
  TopicPublishInfoPtr info = MQClientInstance::topicRouteData2TopicPublishInfo("MetaTopic", route);

  EXPECT_EQ(4, info->getQueueIdByBroker("brokerA"));
  EXPECT_EQ(1, info->getQueueIdByBroker("brokerB"));
  EXPECT_EQ(-1, info->getQueueIdByBroker("missing"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "TopicPublishInfoTest.*";
  return RUN_ALL_TESTS();
}
