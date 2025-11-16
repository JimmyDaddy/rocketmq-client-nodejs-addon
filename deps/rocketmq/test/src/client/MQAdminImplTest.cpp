#include <gtest/gtest.h>

#include <map>

#define private public
#include "MQClientInstance.h"
#undef private

#include "MQAdminImpl.h"
#include "MQClientAPIImpl.h"
#include "MQClientConfigImpl.hpp"
#include "MQException.h"
#include "MQMessageQueue.h"
#include "PermName.h"
#include "protocol/body/TopicRouteData.hpp"

using rocketmq::MQAdminImpl;
using rocketmq::MQClientAPIImpl;
using rocketmq::MQClientConfigImpl;
using rocketmq::MQClientInstance;
using rocketmq::MQClientInstancePtr;
using rocketmq::MQClientException;
using rocketmq::MQMessageQueue;
using rocketmq::PermName;
using rocketmq::TopicRouteData;
using rocketmq::TopicRouteDataPtr;

namespace {

class StubMQClientAPIImpl : public MQClientAPIImpl {
 public:
  explicit StubMQClientAPIImpl(const rocketmq::MQClientConfig& config)
      : MQClientAPIImpl(nullptr, nullptr, config),
        search_offset_result_(0),
        max_offset_result_(0),
        min_offset_result_(0),
        earliest_time_result_(0) {}

  void setTopicRouteData(TopicRouteDataPtr route) { route_data_ = route; }
  void setSearchOffsetResult(int64_t value) { search_offset_result_ = value; }
  void setMaxOffsetResult(int64_t value) { max_offset_result_ = value; }
  void setMinOffsetResult(int64_t value) { min_offset_result_ = value; }
  void setEarliestTimeResult(int64_t value) { earliest_time_result_ = value; }

  struct OffsetCall {
    std::string addr;
    std::string topic;
    int queue_id;
    int64_t arg;
  } last_search_call_{}, last_max_call_{}, last_min_call_{}, last_earliest_call_{};

  TopicRouteData* getTopicRouteInfoFromNameServer(const std::string&, int) override {
    if (!route_data_) {
      return nullptr;
    }
    return new TopicRouteData(*route_data_);
  }

  int64_t searchOffset(const std::string& addr,
                       const std::string& topic,
                       int queueId,
                       int64_t timestamp,
                       int) override {
    last_search_call_ = {addr, topic, queueId, timestamp};
    return search_offset_result_;
  }

  int64_t getMaxOffset(const std::string& addr, const std::string& topic, int queueId, int) override {
    last_max_call_ = {addr, topic, queueId, 0};
    return max_offset_result_;
  }

  int64_t getMinOffset(const std::string& addr, const std::string& topic, int queueId, int) override {
    last_min_call_ = {addr, topic, queueId, 0};
    return min_offset_result_;
  }

  int64_t getEarliestMsgStoretime(const std::string& addr, const std::string& topic, int queueId, int) override {
    last_earliest_call_ = {addr, topic, queueId, 0};
    return earliest_time_result_;
  }

 private:
  TopicRouteDataPtr route_data_;
  int64_t search_offset_result_;
  int64_t max_offset_result_;
  int64_t min_offset_result_;
  int64_t earliest_time_result_;
};

TopicRouteDataPtr BuildRoute(bool readable) {
  auto route = std::make_shared<TopicRouteData>();
  int perm = readable ? (PermName::PERM_READ | PermName::PERM_WRITE) : PermName::PERM_WRITE;
  route->queue_datas().emplace_back("brokerA", 2, 2, perm);
  route->broker_datas().emplace_back("brokerA", std::map<int, std::string>{{rocketmq::MASTER_ID, "1.1.1.1:10911"}});
  return route;
}

class MQAdminImplFixture : public ::testing::Test {
 protected:
  MQAdminImplFixture() {
    config_ = std::make_shared<MQClientConfigImpl>();
    config_->set_namesrv_addr("127.0.0.1:9876");
    config_->set_instance_name("MQAdminImplFixture" + std::to_string(counter_++));
    client_instance_.reset(new MQClientInstance(*config_, config_->buildMQClientId()));
    stub_api_ = new StubMQClientAPIImpl(*config_);
    client_instance_->mq_client_api_impl_.reset(stub_api_);
    admin_.reset(new MQAdminImpl(client_instance_.get()));
  }

  static inline int counter_{0};

  std::shared_ptr<MQClientConfigImpl> config_;
  MQClientInstancePtr client_instance_;
  std::unique_ptr<MQAdminImpl> admin_;
  StubMQClientAPIImpl* stub_api_;
};

}  // namespace

TEST_F(MQAdminImplFixture, FetchSubscribeMessageQueuesReturnsReadableQueues) {
  stub_api_->setTopicRouteData(BuildRoute(true));

  std::vector<MQMessageQueue> queues;
  admin_->fetchSubscribeMessageQueues("TestTopic", queues);

  ASSERT_EQ(2u, queues.size());
  EXPECT_EQ("brokerA", queues[0].broker_name());
  EXPECT_EQ(0, queues[0].queue_id());
}

TEST_F(MQAdminImplFixture, FetchSubscribeMessageQueuesThrowsWhenNamesrvEmpty) {
  stub_api_->setTopicRouteData(BuildRoute(false));

  std::vector<MQMessageQueue> queues;
  EXPECT_THROW(admin_->fetchSubscribeMessageQueues("UnreadableTopic", queues), MQClientException);
}

TEST_F(MQAdminImplFixture, SearchOffsetQueriesActiveBroker) {
  client_instance_->broker_addr_table_["brokerA"][rocketmq::MASTER_ID] = "10.0.0.1:10911";
  stub_api_->setSearchOffsetResult(12345);

  MQMessageQueue mq("TopicA", "brokerA", 1);
  int64_t result = admin_->searchOffset(mq, 9876);

  EXPECT_EQ(12345, result);
  EXPECT_EQ("10.0.0.1:10911", stub_api_->last_search_call_.addr);
  EXPECT_EQ("TopicA", stub_api_->last_search_call_.topic);
  EXPECT_EQ(1, stub_api_->last_search_call_.queue_id);
  EXPECT_EQ(9876, stub_api_->last_search_call_.arg);
}

TEST_F(MQAdminImplFixture, MaxMinAndEarliestOffsetQueriesUseBrokerAddress) {
  client_instance_->broker_addr_table_["brokerX"][rocketmq::MASTER_ID] = "10.0.0.2:10912";
  stub_api_->setMaxOffsetResult(111);
  stub_api_->setMinOffsetResult(5);
  stub_api_->setEarliestTimeResult(42);

  MQMessageQueue mq("TopicB", "brokerX", 0);

  EXPECT_EQ(111, admin_->maxOffset(mq));
  EXPECT_EQ("10.0.0.2:10912", stub_api_->last_max_call_.addr);

  EXPECT_EQ(5, admin_->minOffset(mq));
  EXPECT_EQ("10.0.0.2:10912", stub_api_->last_min_call_.addr);

  EXPECT_EQ(42, admin_->earliestMsgStoreTime(mq));
  EXPECT_EQ("10.0.0.2:10912", stub_api_->last_earliest_call_.addr);
}

TEST_F(MQAdminImplFixture, OffsetQueriesThrowWhenBrokerMissing) {
  MQMessageQueue mq("TopicMissing", "ghostBroker", 0);
  EXPECT_THROW(admin_->maxOffset(mq), MQClientException);
  EXPECT_THROW(admin_->minOffset(mq), MQClientException);
  EXPECT_THROW(admin_->earliestMsgStoreTime(mq), MQClientException);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQAdminImplFixture.*";
  return RUN_ALL_TESTS();
}
