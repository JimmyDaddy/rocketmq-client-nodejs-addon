#include <gtest/gtest.h>

#include <vector>

#include "MQMessageExt.h"
#include "QueryResult.h"

using rocketmq::MQMessageExt;
using rocketmq::QueryResult;

TEST(QueryResultTest, StoresTimestampAndMessageCopies) {
  MQMessageExt msg1;
  msg1.set_queue_id(1);
  msg1.set_queue_offset(42);
  msg1.set_topic("TopicA");

  MQMessageExt msg2;
  msg2.set_queue_id(2);
  msg2.set_queue_offset(84);
  msg2.set_topic("TopicB");

  std::vector<MQMessageExt> messages{msg1, msg2};
  QueryResult result(123456789ULL, messages);

  EXPECT_EQ(123456789ULL, result.index_last_update_timestamp());
  ASSERT_EQ(2u, result.message_list().size());
  EXPECT_EQ(1, result.message_list()[0].queue_id());
  EXPECT_EQ(84, result.message_list()[1].queue_offset());
  EXPECT_EQ("TopicB", result.message_list()[1].topic());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "QueryResultTest.*";
  return RUN_ALL_TESTS();
}
