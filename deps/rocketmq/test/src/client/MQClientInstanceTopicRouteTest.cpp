#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "MQClientInstance.h"
#include "common/PermName.h"
#include "common/UtilAll.h"

using rocketmq::BrokerData;
using rocketmq::MQClientInstance;
using rocketmq::MQMessageQueue;
using rocketmq::PermName;
using rocketmq::TopicPublishInfoPtr;
using rocketmq::TopicRouteData;
using rocketmq::TopicRouteDataPtr;

namespace {

TopicRouteDataPtr BuildOrderRoute(const std::string& conf) {
  auto route = std::make_shared<TopicRouteData>();
  route->set_order_topic_conf(conf);
  return route;
}

TopicRouteDataPtr BuildNormalRoute() {
  auto route = std::make_shared<TopicRouteData>();
  auto& queues = route->queue_datas();
  queues.emplace_back("brokerA", 1, 2, PermName::PERM_READ | PermName::PERM_WRITE);
  queues.emplace_back("brokerB", 4, 3, PermName::PERM_READ);                    // read-only
  queues.emplace_back("brokerC", 2, 2, PermName::PERM_READ | PermName::PERM_WRITE);
  queues.emplace_back("brokerD", 2, 2, PermName::PERM_READ | PermName::PERM_WRITE);

  auto& brokers = route->broker_datas();
  brokers.emplace_back("brokerA", std::map<int, std::string>{{rocketmq::MASTER_ID, "1.1.1.1"}});
  brokers.emplace_back("brokerC", std::map<int, std::string>{{rocketmq::MASTER_ID, "2.2.2.2"}});
  brokers.emplace_back("brokerD", std::map<int, std::string>{{1, "3.3.3.3"}});  // missing master
  return route;
}

}  // namespace

TEST(MQClientInstanceTopicRouteTest, BuildsOrderQueuesFromConf) {
  auto route = BuildOrderRoute("brokerA:2;brokerB:1");
  auto publish = MQClientInstance::topicRouteData2TopicPublishInfo("TestTopic", route);

  ASSERT_TRUE(publish->isOrderTopic());
  const auto& queues = publish->getMessageQueueList();
  ASSERT_EQ(3u, queues.size());
  EXPECT_EQ("brokerA", queues[0].broker_name());
  EXPECT_EQ(0, queues[0].queue_id());
  EXPECT_EQ("brokerB", queues.back().broker_name());
  EXPECT_EQ(0, queues.back().queue_id());
}

TEST(MQClientInstanceTopicRouteTest, FiltersNonWritableOrMasterlessQueues) {
  auto route = BuildNormalRoute();
  auto publish = MQClientInstance::topicRouteData2TopicPublishInfo("MixTopic", route);

  ASSERT_FALSE(publish->isOrderTopic());
  const auto& queues = publish->getMessageQueueList();
  // brokerA: queue ids 0,1 ; brokerC: 0,1 ; brokerD skipped (missing master) ; brokerB skipped (not writable)
  ASSERT_EQ(4u, queues.size());
  std::vector<std::pair<std::string, int>> actual;
  for (const auto& mq : queues) {
    actual.emplace_back(mq.broker_name(), mq.queue_id());
  }
  std::sort(actual.begin(), actual.end());
  std::vector<std::pair<std::string, int>> expected = {
      {"brokerA", 0}, {"brokerA", 1}, {"brokerC", 0}, {"brokerC", 1}};
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(expected, actual);
}

TEST(MQClientInstanceTopicRouteTest, SubscribeInfoContainsOnlyReadableQueues) {
  auto route = BuildNormalRoute();
  auto mqs = MQClientInstance::topicRouteData2TopicSubscribeInfo("SubTopic", route);

  std::map<std::string, int> counts;
  for (const auto& mq : mqs) {
    counts[mq.broker_name()]++;
  }
  EXPECT_EQ(1 + 4 + 2 + 2, static_cast<int>(mqs.size()));
  EXPECT_EQ(1, counts["brokerA"]);
  EXPECT_EQ(4, counts["brokerB"]);
  EXPECT_EQ(2, counts["brokerC"]);
  EXPECT_EQ(2, counts["brokerD"]);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQClientInstanceTopicRouteTest.*";
  return RUN_ALL_TESTS();
}
