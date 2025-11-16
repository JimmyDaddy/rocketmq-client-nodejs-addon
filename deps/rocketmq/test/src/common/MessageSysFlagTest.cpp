#include <gtest/gtest.h>

#include "MessageSysFlag.h"

using rocketmq::MessageSysFlag;

TEST(MessageSysFlagTest, TransactionValuesRoundTrip) {
  int flag = 0;
  flag = MessageSysFlag::resetTransactionValue(flag, MessageSysFlag::TRANSACTION_PREPARED_TYPE);
  EXPECT_EQ(MessageSysFlag::TRANSACTION_PREPARED_TYPE, MessageSysFlag::getTransactionValue(flag));

  flag = MessageSysFlag::resetTransactionValue(flag, MessageSysFlag::TRANSACTION_COMMIT_TYPE);
  EXPECT_EQ(MessageSysFlag::TRANSACTION_COMMIT_TYPE, MessageSysFlag::getTransactionValue(flag));
}

TEST(MessageSysFlagTest, ClearCompressedFlag) {
  int flag = MessageSysFlag::COMPRESSED_FLAG | MessageSysFlag::BORNHOST_V6_FLAG;
  int cleared = MessageSysFlag::clearCompressedFlag(flag);
  EXPECT_EQ(MessageSysFlag::BORNHOST_V6_FLAG, cleared);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MessageSysFlagTest.*";
  return RUN_ALL_TESTS();
}
