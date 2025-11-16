#include <gtest/gtest.h>

#include <future>
#include <string>
#include <thread>

#include "CErrorContainer.h"
#include "c/CErrorMessage.h"

using rocketmq::CErrorContainer;

TEST(CErrorContainerTest, SetAndGetThroughBothAPIs) {
  CErrorContainer::setErrorMessage("first-error");
  EXPECT_EQ(std::string("first-error"), CErrorContainer::getErrorMessage());
  EXPECT_STREQ("first-error", GetLatestErrorMessage());

  std::string movable = "second-error";
  CErrorContainer::setErrorMessage(std::move(movable));
  EXPECT_TRUE(movable.empty());
  EXPECT_EQ(std::string("second-error"), CErrorContainer::getErrorMessage());
  EXPECT_STREQ("second-error", GetLatestErrorMessage());
}

TEST(CErrorContainerTest, MaintainsThreadLocalStoragePerThread) {
  CErrorContainer::setErrorMessage("main-thread");

  std::promise<std::string> worker_result_promise;
  auto worker_result = worker_result_promise.get_future();

  std::thread worker(
      [](std::promise<std::string> promise) {
        CErrorContainer::setErrorMessage("worker-thread");
        promise.set_value(CErrorContainer::getErrorMessage());
      },
      std::move(worker_result_promise));

  const std::string worker_message = worker_result.get();
  worker.join();

  EXPECT_EQ("worker-thread", worker_message);
  EXPECT_EQ(std::string("main-thread"), CErrorContainer::getErrorMessage());
  EXPECT_STREQ("main-thread", GetLatestErrorMessage());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "CErrorContainerTest.*";
  return RUN_ALL_TESTS();
}
