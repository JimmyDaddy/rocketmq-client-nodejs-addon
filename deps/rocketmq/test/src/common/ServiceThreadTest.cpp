#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

#include "ServiceThread.h"

using rocketmq::ServiceThread;

namespace {

class TestServiceThread : public ServiceThread {
 public:
  TestServiceThread() : iterations_(0), wait_end_calls_(0), wait_interval_ms_(1000) {}

  std::string getServiceName() override { return "TestServiceThread"; }

  void run() override {
    while (!isStopped()) {
      waitForRunning(wait_interval_ms_.load());
      iterations_.fetch_add(1);
    }
  }

  void onWaitEnd() override { wait_end_calls_.fetch_add(1); }

  void setWaitIntervalMs(int interval) { wait_interval_ms_.store(interval); }
  int iterations() const { return iterations_.load(); }
  int waitEndCalls() const { return wait_end_calls_.load(); }

 private:
  std::atomic<int> iterations_;
  std::atomic<int> wait_end_calls_;
  std::atomic<int> wait_interval_ms_;
};

bool WaitForCondition(const std::function<bool()>& predicate, int retries = 100, int sleep_ms = 10) {
  for (int i = 0; i < retries; ++i) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
  return predicate();
}

}  // namespace

TEST(ServiceThreadTest, StartAndShutdownStopsThread) {
  TestServiceThread service;
  service.setWaitIntervalMs(50);
  service.start();
  service.wakeup();

  ASSERT_TRUE(WaitForCondition([&] { return service.iterations() > 0; }));

  service.shutdown();
  EXPECT_TRUE(service.isStopped());
}

TEST(ServiceThreadTest, WakeupNotifiesWaitingThread) {
  TestServiceThread service;
  service.setWaitIntervalMs(1000);
  service.start();

  service.wakeup();
  ASSERT_TRUE(WaitForCondition([&] { return service.waitEndCalls() >= 1; }, 50, 5));

  int previous = service.waitEndCalls();
  service.wakeup();
  ASSERT_TRUE(WaitForCondition([&] { return service.waitEndCalls() >= previous + 1; }, 50, 5));

  service.shutdown();
}

TEST(ServiceThreadTest, TimeoutTriggersWaitEnd) {
  TestServiceThread service;
  service.setWaitIntervalMs(20);
  service.start();

  ASSERT_TRUE(WaitForCondition([&] { return service.waitEndCalls() >= 1; }, 100, 5));

  service.shutdown();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ServiceThreadTest.*";
  return RUN_ALL_TESTS();
}
