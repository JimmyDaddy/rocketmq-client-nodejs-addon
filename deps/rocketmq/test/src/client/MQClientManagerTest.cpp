#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "MQClientConfigImpl.hpp"
#include "MQClientManager.h"

using rocketmq::MQClientConfigImpl;
using rocketmq::MQClientInstancePtr;
using rocketmq::MQClientManager;

namespace {

std::string UniqueInstanceName(const std::string& suffix) {
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return "MQClientManagerTest-" + suffix + "-" + std::to_string(now);
}

MQClientConfigImpl MakeConfig(const std::string& suffix) {
  MQClientConfigImpl config;
  config.set_group_name("TestGroup");
  config.set_namesrv_addr("127.0.0.1:9876");
  config.set_instance_name(UniqueInstanceName(suffix));
  return config;
}

}  // namespace

TEST(MQClientManagerTest, SingletonReturnsSamePointer) {
  auto* first = MQClientManager::getInstance();
  auto* second = MQClientManager::getInstance();
  ASSERT_NE(nullptr, first);
  EXPECT_EQ(first, second);
}

TEST(MQClientManagerTest, ReusesCachedInstanceUntilRemoved) {
  auto* manager = MQClientManager::getInstance();
  auto config = MakeConfig("reuse");
  auto client_id = config.buildMQClientId();

  auto first = manager->getOrCreateMQClientInstance(config);
  ASSERT_NE(nullptr, first);

  auto second = manager->getOrCreateMQClientInstance(config);
  ASSERT_NE(nullptr, second);
  EXPECT_EQ(first.get(), second.get()) << "Should reuse cached MQClientInstance";

  std::weak_ptr<rocketmq::MQClientInstance> cached = first;
  first.reset();
  second.reset();
  manager->removeMQClientInstance(client_id);

  EXPECT_TRUE(cached.expired()) << "Removal should release the cached instance";

  auto recreated = manager->getOrCreateMQClientInstance(config);
  ASSERT_NE(nullptr, recreated);

  recreated.reset();
  manager->removeMQClientInstance(client_id);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQClientManagerTest.*";
  return RUN_ALL_TESTS();
}
