#include <gtest/gtest.h>

#include <memory>

#include "MQClientInstance.h"
#include "MQMessageQueue.h"
#include "UtilAll.h"
#include "common/PermName.h"

using rocketmq::BrokerData;
using rocketmq::MQClientInstance;
using rocketmq::MQMessageQueue;
using rocketmq::PermName;
using rocketmq::TopicPublishInfoPtr;
using rocketmq::TopicRouteData;
using rocketmq::TopicRouteDataPtr;

namespace {

TopicRouteDataPtr makeRoute(const std::string& order_conf = std::string()) {
  auto route = std::make_shared<TopicRouteData>();
  route->set_order_topic_conf(order_conf);
  return route;
}

}  // namespace

TEST(TopicRouteBuilderTest, BuildsOrderTopicFromConf) {
  auto route = makeRoute("brokerA:2;brokerB:1");

  auto info = MQClientInstance::topicRouteData2TopicPublishInfo("OrderTopic", route);
  ASSERT_TRUE(info->isOrderTopic());

  const auto& queues = info->getMessageQueueList();
  ASSERT_EQ(3u, queues.size());
  EXPECT_EQ("brokerA", queues[0].broker_name());
  EXPECT_EQ(0, queues[0].queue_id());
  EXPECT_EQ("brokerA", queues[1].broker_name());
  EXPECT_EQ(1, queues[1].queue_id());
  EXPECT_EQ("brokerB", queues[2].broker_name());
  EXPECT_EQ(0, queues[2].queue_id());
}

TEST(TopicRouteBuilderTest, FiltersNonWritableQueuesAndSortsByQueueId) {
  auto route = makeRoute();
  route->queue_datas().emplace_back("brokerB", 2, 2, PermName::PERM_READ | PermName::PERM_WRITE);
  route->queue_datas().emplace_back("brokerA", 2, 2, PermName::PERM_READ | PermName::PERM_WRITE);
  route->queue_datas().emplace_back("brokerC", 2, 2, PermName::PERM_READ);  // read-only -> skipped

  BrokerData brokerA("brokerA");
  brokerA.broker_addrs()[rocketmq::MASTER_ID] = "127.0.0.1:10911";
  route->broker_datas().push_back(brokerA);

  BrokerData brokerB("brokerB");
  brokerB.broker_addrs()[rocketmq::MASTER_ID] = "127.0.0.1:10912";
  route->broker_datas().push_back(brokerB);

  BrokerData brokerC("brokerC");
  brokerC.broker_addrs()[rocketmq::MASTER_ID] = "127.0.0.1:10913";
  route->broker_datas().push_back(brokerC);

  auto info = MQClientInstance::topicRouteData2TopicPublishInfo("NormalTopic", route);
  ASSERT_FALSE(info->isOrderTopic());

  const auto& queues = info->getMessageQueueList();
  ASSERT_EQ(4u, queues.size());
  EXPECT_EQ("brokerA", queues[0].broker_name());
  EXPECT_EQ(0, queues[0].queue_id());
  EXPECT_EQ("brokerB", queues[1].broker_name());
  EXPECT_EQ(0, queues[1].queue_id());
  EXPECT_EQ("brokerA", queues[2].broker_name());
  EXPECT_EQ(1, queues[2].queue_id());
  EXPECT_EQ("brokerB", queues[3].broker_name());
  EXPECT_EQ(1, queues[3].queue_id());
}

TEST(TopicRouteBuilderTest, BuildsSubscribeInfoForReadableQueues) {
  auto route = makeRoute();
  route->queue_datas().emplace_back("brokerA", 3, 1, PermName::PERM_READ);
  route->queue_datas().emplace_back("brokerB", 3, 1, PermName::PERM_WRITE);

  auto subscribe = MQClientInstance::topicRouteData2TopicSubscribeInfo("SubTopic", route);
  ASSERT_EQ(3u, subscribe.size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ("brokerA", subscribe[i].broker_name());
    EXPECT_EQ(i, subscribe[i].queue_id());
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "TopicRouteBuilderTest.*";
  return RUN_ALL_TESTS();
}
