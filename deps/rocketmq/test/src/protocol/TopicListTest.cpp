#include <gtest/gtest.h>

#include "ByteArray.h"

#define private public
#include "protocol/TopicList.h"
#undef private

using rocketmq::ByteArray;
using rocketmq::TopicList;

TEST(TopicListTest, DecodeReturnsEmptyListForArbitraryPayload) {
  const char raw[] = "random-bytes";
  ByteArray buffer(const_cast<char*>(raw), sizeof(raw) - 1);

  std::unique_ptr<TopicList> decoded(TopicList::Decode(buffer));
  ASSERT_NE(nullptr, decoded);
  EXPECT_TRUE(decoded->topic_list_.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "TopicListTest.*";
  return RUN_ALL_TESTS();
}
