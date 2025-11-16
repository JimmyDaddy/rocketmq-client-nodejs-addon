#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <vector>

#define private public
#define protected public
#include "consumer/RebalanceImpl.h"
#undef private
#undef protected

#include "PullRequest.h"
#include "AllocateMQStrategy.h"
#include "ProcessQueue.h"
#include "UtilAll.h"
#include "protocol/heartbeat/SubscriptionData.hpp"

namespace rocketmq {
namespace {

class StubAllocateStrategy : public AllocateMQStrategy {
 public:
  void allocate(const std::string& currentCID,
                std::vector<MQMessageQueue>& mqAll,
                std::vector<std::string>& cidAll,
                std::vector<MQMessageQueue>& outResult) override {
    recorded_current_cid = currentCID;
    recorded_mq_all = mqAll;
    recorded_cid_all = cidAll;
    outResult = planned_result;
  }

  std::string recorded_current_cid;
  std::vector<MQMessageQueue> recorded_mq_all;
  std::vector<std::string> recorded_cid_all;
  std::vector<MQMessageQueue> planned_result;
};

class TestRebalanceImpl : public RebalanceImpl {
 public:
  TestRebalanceImpl(const std::string& group,
                    MessageModel model,
                    AllocateMQStrategy* strategy)
      : RebalanceImpl(group, model, strategy, nullptr) {}

  ConsumeType consumeType() override { return consume_type; }

  bool removeUnnecessaryMessageQueue(const MQMessageQueue& mq, ProcessQueuePtr) override {
    removed_mqs.push_back(mq);
    return allow_remove;
  }

  void removeDirtyOffset(const MQMessageQueue& mq) override { dirty_offsets.push_back(mq); }

  int64_t computePullFromWhere(const MQMessageQueue& mq) override {
    auto it = next_offsets.find(mq.toString());
    return it == next_offsets.end() ? default_offset : it->second;
  }

  void dispatchPullRequest(const std::vector<PullRequestPtr>& requests) override { dispatched = requests; }

  void messageQueueChanged(const std::string& topic,
                           std::vector<MQMessageQueue>& mqAll,
                           std::vector<MQMessageQueue>& mqDivided) override {
    ++message_queue_changed_calls;
    last_topic = topic;
    last_mq_all = mqAll;
    last_mq_divided = mqDivided;
  }

  ConsumeType consume_type = CONSUME_PASSIVELY;
  bool allow_remove = true;
  int64_t default_offset = 0;
  std::map<std::string, int64_t> next_offsets;
  std::vector<MQMessageQueue> removed_mqs;
  std::vector<MQMessageQueue> dirty_offsets;
  std::vector<PullRequestPtr> dispatched;
  int message_queue_changed_calls = 0;
  std::string last_topic;
  std::vector<MQMessageQueue> last_mq_all;
  std::vector<MQMessageQueue> last_mq_divided;
};

MQMessageQueue makeQueue(const std::string& topic, const std::string& broker, int id) {
  return MQMessageQueue(topic, broker, id);
}

TEST(RebalanceImplTest, UpdateProcessQueueAddsNewQueues) {
  StubAllocateStrategy strategy;
  TestRebalanceImpl impl("groupA", CLUSTERING, &strategy);

  MQMessageQueue mq = makeQueue("TopicA", "BrokerA", 0);
  impl.next_offsets[mq.toString()] = 12;
  std::vector<MQMessageQueue> mqSet = {mq};

  bool changed = impl.updateProcessQueueTableInRebalance("TopicA", mqSet, false);

  EXPECT_TRUE(changed);
  auto table = impl.getProcessQueueTable();
  ASSERT_EQ(1u, table.size());
  ASSERT_EQ(1u, impl.dispatched.size());
  EXPECT_EQ(12, impl.dispatched.front()->next_offset());
  EXPECT_EQ(mq, impl.dispatched.front()->message_queue());
}

TEST(RebalanceImplTest, UpdateProcessQueueRemovesUnusedQueues) {
  StubAllocateStrategy strategy;
  TestRebalanceImpl impl("groupB", CLUSTERING, &strategy);

  MQMessageQueue mq = makeQueue("TopicB", "BrokerB", 1);
  ProcessQueuePtr pq(new ProcessQueue());
  impl.putProcessQueueIfAbsent(mq, pq);

  std::vector<MQMessageQueue> empty;
  bool changed = impl.updateProcessQueueTableInRebalance("TopicB", empty, false);

  EXPECT_TRUE(changed);
  EXPECT_EQ(0u, impl.getProcessQueueTable().size());
  ASSERT_EQ(1u, impl.removed_mqs.size());
  EXPECT_EQ(mq, impl.removed_mqs.front());
}

TEST(RebalanceImplTest, BuildProcessQueueTableGroupsByBroker) {
  StubAllocateStrategy strategy;
  TestRebalanceImpl impl("groupC", CLUSTERING, &strategy);

  impl.putProcessQueueIfAbsent(makeQueue("Topic", "BrokerA", 0), std::make_shared<ProcessQueue>());
  impl.putProcessQueueIfAbsent(makeQueue("Topic", "BrokerA", 1), std::make_shared<ProcessQueue>());
  impl.putProcessQueueIfAbsent(makeQueue("Topic", "BrokerB", 0), std::make_shared<ProcessQueue>());

  auto broker_map = impl.buildProcessQueueTableByBrokerName();
  ASSERT_EQ(2u, broker_map->size());
  EXPECT_EQ(2u, broker_map->at("BrokerA").size());
  EXPECT_EQ(1u, broker_map->at("BrokerB").size());
}

TEST(RebalanceImplTest, TruncateMessageQueueNotMyTopicDropsEntries) {
  StubAllocateStrategy strategy;
  TestRebalanceImpl impl("groupD", CLUSTERING, &strategy);

  auto pq = std::make_shared<ProcessQueue>();
  MQMessageQueue stale = makeQueue("OrphanTopic", "BrokerC", 0);
  impl.putProcessQueueIfAbsent(stale, pq);

  auto* sub = new SubscriptionData("KeptTopic", "*");
  impl.subscription_inner_["KeptTopic"] = sub;

  impl.truncateMessageQueueNotMyTopic();

  EXPECT_EQ(0u, impl.getProcessQueueTable().size());
  EXPECT_TRUE(pq->dropped());
}

}  // namespace
}  // namespace rocketmq
