#include <gtest/gtest.h>

#include <memory>

#include "producer/RequestFutureTable.h"
#include "producer/RequestResponseFuture.h"

using rocketmq::RequestFutureTable;
using rocketmq::RequestResponseFuture;

TEST(RequestFutureTableTest, PutAndRemoveReturnsSameFuture) {
  auto future = std::make_shared<RequestResponseFuture>("corr-1", 3000, nullptr);
  RequestFutureTable::putRequestFuture("corr-1", future);

  auto stored = RequestFutureTable::removeRequestFuture("corr-1");
  EXPECT_EQ(future, stored);
}

TEST(RequestFutureTableTest, RemoveUnknownIdReturnsNullptr) {
  auto result = RequestFutureTable::removeRequestFuture("missing-id");
  EXPECT_EQ(nullptr, result);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "RequestFutureTableTest.*";
  return RUN_ALL_TESTS();
}
