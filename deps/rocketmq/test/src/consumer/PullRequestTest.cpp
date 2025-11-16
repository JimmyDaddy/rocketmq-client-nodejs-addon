#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "MQMessageQueue.h"
#include "consumer/ProcessQueue.h"
#include "consumer/PullRequest.h"

using rocketmq::MQMessageQueue;
using rocketmq::ProcessQueue;
using rocketmq::ProcessQueuePtr;
using rocketmq::PullRequest;

TEST(PullRequestTest, SettersStoreState) {
  PullRequest request;
  request.set_consumer_group("groupA");
  MQMessageQueue mq("TopicA", "broker-a", 2);
  request.set_message_queue(mq);
  request.set_next_offset(12345);
  request.set_locked_first(true);
  ProcessQueuePtr queue = std::make_shared<ProcessQueue>();
  request.set_process_queue(queue);

  EXPECT_EQ("groupA", request.consumer_group());
  EXPECT_EQ(mq.toString(), request.message_queue().toString());
  EXPECT_EQ(12345, request.next_offset());
  EXPECT_TRUE(request.locked_first());
  EXPECT_EQ(queue, request.process_queue());
}

TEST(PullRequestTest, ToStringIncludesKeyFields) {
  PullRequest request;
  request.set_consumer_group("groupB");
  MQMessageQueue mq("TopicB", "broker-b", 1);
  request.set_message_queue(mq);
  request.set_next_offset(99);

  std::string summary = request.toString();
  EXPECT_NE(std::string::npos, summary.find("groupB"));
  EXPECT_NE(std::string::npos, summary.find(mq.toString()));
  EXPECT_NE(std::string::npos, summary.find("nextOffset=99"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "PullRequestTest.*";
  return RUN_ALL_TESTS();
}
