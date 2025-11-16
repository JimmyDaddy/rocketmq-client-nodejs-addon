#include <atomic>
#include <gtest/gtest.h>

#include "MQClientImpl.h"
#include "MQClientInstance.h"
#include "MQClientManager.h"
#include "MQException.h"
#include "ServiceState.h"
#include "MQClientConfigImpl.hpp"

using rocketmq::MQClientConfigImpl;
using rocketmq::MQClientException;
using rocketmq::MQClientImpl;
using rocketmq::MQClientInstance;
using rocketmq::MQClientInstancePtr;
using rocketmq::MQClientManager;
using rocketmq::MQClientConfigPtr;
using rocketmq::ServiceState;

namespace {

std::string UniqueInstanceName(const std::string& prefix) {
  static std::atomic<int> counter{0};
  return prefix + std::to_string(counter.fetch_add(1));
}

MQClientConfigPtr MakeConfig(const std::string& prefix) {
  auto config = std::make_shared<MQClientConfigImpl>();
  config->set_namesrv_addr("127.0.0.1:9876");
  config->set_instance_name(UniqueInstanceName(prefix));
  config->set_group_name(prefix + "Group");
  return config;
}

class TestableMQClientImpl : public MQClientImpl {
 public:
  using MQClientImpl::MQClientImpl;

  void ForceServiceState(ServiceState state) { service_state_ = state; }
};

}  // namespace

TEST(MQClientImplTest, StartThrowsWithoutConfig) {
  MQClientImpl client(MQClientConfigPtr(), nullptr);
  EXPECT_THROW(client.start(), MQClientException);
  client.shutdown();
}

TEST(MQClientImplTest, StartInitializesClientInstance) {
  auto config = MakeConfig("Start");
  MQClientImpl client(config, nullptr);

  client.start();

  auto instance = client.getClientInstance();
  ASSERT_NE(instance, nullptr);
  const std::string client_id = instance->getClientId();

  client.shutdown();
  MQClientManager::getInstance()->removeMQClientInstance(client_id);
}

TEST(MQClientImplTest, SetClientInstanceSucceedsBeforeStart) {
  auto config = MakeConfig("AssignBeforeStart");
  TestableMQClientImpl client(config, nullptr);

  MQClientInstancePtr manual_instance(new MQClientInstance(*config, config->buildMQClientId()));

  client.setClientInstance(manual_instance);
  EXPECT_EQ(client.getClientInstance(), manual_instance);

  client.shutdown();
}

TEST(MQClientImplTest, SetClientInstanceFailsWhenRunning) {
  auto config = MakeConfig("AssignAfterStart");
  TestableMQClientImpl client(config, nullptr);

  auto managed_instance = MQClientManager::getInstance()->getOrCreateMQClientInstance(*config);
  client.ForceServiceState(ServiceState::RUNNING);

  EXPECT_THROW(client.setClientInstance(managed_instance), MQClientException);

  MQClientManager::getInstance()->removeMQClientInstance(managed_instance->getClientId());
  client.shutdown();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQClientImplTest.*";
  return RUN_ALL_TESTS();
}
