#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>

#define private public
#include "MQClientInstance.h"
#undef private

#include "MQClientConfigImpl.hpp"
#include "ServiceState.h"
#include "consumer/PullMessageService.hpp"
#include "consumer/PullRequest.h"

using rocketmq::MQClientConfigImpl;
using rocketmq::MQClientInstance;
using rocketmq::MQClientInstancePtr;
using rocketmq::PullMessageService;
using rocketmq::PullRequest;
using rocketmq::PullRequestPtr;
using rocketmq::ServiceState;

namespace {

class TestPullMessageService : public PullMessageService {
 public:
  explicit TestPullMessageService(MQClientInstance* instance) : PullMessageService(instance) {}

  void pullMessage(PullRequestPtr pullRequest) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      last_group_ = pullRequest->consumer_group();
      ++invocation_count_;
    }
    cv_.notify_all();
  }

  bool WaitForInvocations(size_t expected, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, timeout, [&] { return invocation_count_ >= expected; });
  }

  size_t invocation_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return invocation_count_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  size_t invocation_count_{0};
  std::string last_group_;
};

MQClientInstancePtr MakeClientInstance(const std::string& name) {
  auto config = std::make_shared<MQClientConfigImpl>();
  config->set_namesrv_addr("127.0.0.1:9876");
  config->set_instance_name(name);
  return std::make_shared<MQClientInstance>(*config, config->buildMQClientId());
}

PullRequestPtr MakePullRequest(const std::string& group) {
  auto request = std::make_shared<PullRequest>();
  request->set_consumer_group(group);
  return request;
}

}  // namespace

TEST(PullMessageServiceTest, SchedulesRequestsWhenClientRunning) {
  auto client = MakeClientInstance("PullMessageServiceTestRunning");
  client->service_state_ = ServiceState::RUNNING;

  TestPullMessageService service(client.get());
  service.start();

  service.executePullRequestLater(MakePullRequest("groupA"), 0);

  EXPECT_TRUE(service.WaitForInvocations(1, std::chrono::milliseconds(200)));

  service.shutdown();
}

TEST(PullMessageServiceTest, SkipsSchedulingWhenClientStopped) {
  auto client = MakeClientInstance("PullMessageServiceTestStopped");
  client->service_state_ = ServiceState::CREATE_JUST;

  TestPullMessageService service(client.get());
  service.start();

  service.executePullRequestLater(MakePullRequest("groupB"), 0);

  EXPECT_FALSE(service.WaitForInvocations(1, std::chrono::milliseconds(200)));
  EXPECT_EQ(0u, service.invocation_count());

  service.shutdown();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "PullMessageServiceTest.*";
  return RUN_ALL_TESTS();
}
