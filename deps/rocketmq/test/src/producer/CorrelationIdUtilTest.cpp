#include <gtest/gtest.h>

#include <algorithm>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "producer/CorrelationIdUtil.hpp"

using rocketmq::CorrelationIdUtil;

TEST(CorrelationIdUtilTest, GeneratesMonotonicIds) {
  auto id1 = CorrelationIdUtil::createCorrelationId();
  auto id2 = CorrelationIdUtil::createCorrelationId();
  auto id3 = CorrelationIdUtil::createCorrelationId();

  auto v1 = std::stoull(id1);
  auto v2 = std::stoull(id2);
  auto v3 = std::stoull(id3);

  EXPECT_LT(v1, v2);
  EXPECT_LT(v2, v3);
}

TEST(CorrelationIdUtilTest, ProvidesUniqueIdsAcrossThreads) {
  constexpr int kThreads = 4;
  constexpr int kIdsPerThread = 256;

  std::vector<std::string> ids;
  ids.reserve(kThreads * kIdsPerThread);
  std::mutex mtx;
  std::vector<std::thread> threads;

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&]() {
      std::vector<std::string> local;
      local.reserve(kIdsPerThread);
      for (int i = 0; i < kIdsPerThread; ++i) {
        local.emplace_back(CorrelationIdUtil::createCorrelationId());
      }
      std::lock_guard<std::mutex> lock(mtx);
      ids.insert(ids.end(), local.begin(), local.end());
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  std::set<std::string> unique(ids.begin(), ids.end());
  ASSERT_EQ(ids.size(), unique.size());

  std::vector<uint64_t> numeric_ids;
  numeric_ids.reserve(ids.size());
  for (const auto& id : ids) {
    numeric_ids.push_back(std::stoull(id));
  }
  std::sort(numeric_ids.begin(), numeric_ids.end());

  for (size_t i = 1; i < numeric_ids.size(); ++i) {
    EXPECT_EQ(numeric_ids[i - 1] + 1, numeric_ids[i]);
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "CorrelationIdUtilTest.*";
  return RUN_ALL_TESTS();
}
