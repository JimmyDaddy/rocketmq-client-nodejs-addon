/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gtest/gtest.h>

#include <memory>

#include "ByteArray.h"
#include "transport/TcpTransport.h"

using rocketmq::ByteArrayRef;
using rocketmq::TCP_CONNECT_STATUS_CLOSED;
using rocketmq::TcpTransport;
using rocketmq::TcpTransportInfo;
using rocketmq::TcpTransportPtr;

namespace {

std::shared_ptr<TcpTransport> MakeTransport(bool* closed_flag = nullptr) {
  auto read_callback = [](ByteArrayRef, TcpTransportPtr) {};
  auto close_callback = [closed_flag](TcpTransportPtr) {
    if (closed_flag != nullptr) {
      *closed_flag = true;
    }
  };
  return TcpTransport::CreateTransport(read_callback, close_callback, std::unique_ptr<TcpTransportInfo>());
}

}  // namespace

TEST(TcpTransportTest, ConnectFailureTransitionsToClosedState) {
  bool close_called = false;
  auto transport = MakeTransport(&close_called);

  auto status = transport->connect("127.0.0.1:badport", /*timeoutMillis=*/50);
  EXPECT_EQ(TCP_CONNECT_STATUS_CLOSED, status);
  EXPECT_EQ(TCP_CONNECT_STATUS_CLOSED, transport->getTcpConnectStatus());
  EXPECT_TRUE(close_called);

  // waitTcpConnectEvent should return immediately once closed
  EXPECT_EQ(TCP_CONNECT_STATUS_CLOSED, transport->waitTcpConnectEvent(10));
}

TEST(TcpTransportTest, SendMessageFailsUntilConnectedAndDisconnectIsIdempotent) {
  auto transport = MakeTransport();

  const char payload[] = "noop";
  EXPECT_FALSE(transport->sendMessage(payload, sizeof(payload)));
  EXPECT_TRUE(transport->getPeerAddrAndPort().empty());

  transport->disconnect("unused");
  EXPECT_EQ(TCP_CONNECT_STATUS_CLOSED, transport->getTcpConnectStatus());

  // second disconnect should be harmless
  transport->disconnect("unused");
  EXPECT_EQ(TCP_CONNECT_STATUS_CLOSED, transport->getTcpConnectStatus());

  // after disconnect, sendMessage should still be guarded
  EXPECT_FALSE(transport->sendMessage(payload, sizeof(payload)));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "TcpTransportTest.*";
  return RUN_ALL_TESTS();
}
