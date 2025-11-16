#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "concurrent/latch.hpp"

using rocketmq::latch;

TEST(LatchTest, WaitersReleaseAfterCountdownCompletes) {
  latch sync_point(2);
  std::atomic<bool> released{false};

  std::thread waiter([&] {
    sync_point.wait();
    released = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_FALSE(released.load());

  sync_point.count_down();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_FALSE(released.load());

  sync_point.count_down();
  waiter.join();
  EXPECT_TRUE(released.load());
}

TEST(LatchTest, TimedWaitExpiresWithoutCountdown) {
  latch sync_point(1);
  std::chrono::steady_clock::time_point start;
  std::chrono::steady_clock::time_point finish;

  std::thread waiter([&] {
    start = std::chrono::steady_clock::now();
    sync_point.wait(50, rocketmq::milliseconds);
    finish = std::chrono::steady_clock::now();
  });

  waiter.join();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
  EXPECT_GE(elapsed.count(), 45);
  EXPECT_FALSE(sync_point.is_ready());
}

TEST(LatchTest, ResetRestoresInitialCount) {
  latch sync_point(2);
  sync_point.count_down();
  EXPECT_FALSE(sync_point.is_ready());

  sync_point.reset();
  EXPECT_FALSE(sync_point.is_ready());

  std::atomic<bool> worker_done{false};
  std::thread worker([&] {
    sync_point.count_down_and_wait();
    worker_done = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_FALSE(worker_done.load());

  sync_point.count_down();
  sync_point.count_down();
  worker.join();
  EXPECT_TRUE(worker_done.load());
  EXPECT_TRUE(sync_point.is_ready());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "LatchTest.*";
  return RUN_ALL_TESTS();
}
