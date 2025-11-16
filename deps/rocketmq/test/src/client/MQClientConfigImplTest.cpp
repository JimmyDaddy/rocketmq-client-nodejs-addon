#include <gtest/gtest.h>

#include "MQClientConfigImpl.hpp"
#include "SocketUtil.h"
#include "UtilAll.h"

using rocketmq::MQClientConfigImpl;

TEST(MQClientConfigImplTest, BuildsClientIdWithUnitName) {
  MQClientConfigImpl config;
  config.set_instance_name("inst");
  config.set_unit_name("unit");

  const std::string expected = rocketmq::GetLocalAddress() + "@inst@unit";
  EXPECT_EQ(expected, config.buildMQClientId());
}

TEST(MQClientConfigImplTest, NameserverFormattingStripsHttpPrefix) {
  MQClientConfigImpl config;
  config.set_namesrv_addr("http://localhost:9876");
  EXPECT_EQ("localhost:9876", config.namesrv_addr());

  config.set_namesrv_addr("another-host:80");
  EXPECT_EQ("another-host:80", config.namesrv_addr());
}

TEST(MQClientConfigImplTest, TcpWorkerThreadsOnlyGrow) {
  MQClientConfigImpl config;
  const int initial = config.tcp_transport_worker_thread_nums();

  config.set_tcp_transport_worker_thread_nums(initial - 1);
  EXPECT_EQ(initial, config.tcp_transport_worker_thread_nums());

  config.set_tcp_transport_worker_thread_nums(initial + 3);
  EXPECT_EQ(initial + 3, config.tcp_transport_worker_thread_nums());
}

TEST(MQClientConfigImplTest, TryLockTimeoutRoundsDownToSecondsWithFloor) {
  MQClientConfigImpl config;

  config.set_tcp_transport_try_lock_timeout(500);
  EXPECT_EQ(1u, config.tcp_transport_try_lock_timeout());

  config.set_tcp_transport_try_lock_timeout(2500);
  EXPECT_EQ(2u, config.tcp_transport_try_lock_timeout());
}

TEST(MQClientConfigImplTest, ChangeInstanceNameUsesPidOnce) {
  MQClientConfigImpl config;
  config.changeInstanceNameToPID();

  const auto pid_string = rocketmq::UtilAll::to_string(rocketmq::UtilAll::getProcessId());
  EXPECT_EQ(pid_string, config.instance_name());

  config.set_instance_name("custom");
  config.changeInstanceNameToPID();
  EXPECT_EQ("custom", config.instance_name());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQClientConfigImplTest.*";
  return RUN_ALL_TESTS();
}
