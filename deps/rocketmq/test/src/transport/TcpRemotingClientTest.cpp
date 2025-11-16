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

#include <vector>

#include "TcpRemotingClient.h"

using rocketmq::TcpRemotingClient;

TEST(TcpRemotingClientTest, UpdateNameServerAddressListFiltersInvalidEntries) {
  TcpRemotingClient client(1, 1000, 1);
  client.updateNameServerAddressList(" 127.0.0.1:9876 ; invalid-entry ; example.com:10911 ; foo ; host:0 ; localhost:9876 ");

  const std::vector<std::string> expected = {"127.0.0.1:9876", "example.com:10911", "localhost:9876"};
  EXPECT_EQ(expected, client.getNameServerAddressList());
}

TEST(TcpRemotingClientTest, UpdateNameServerAddressListOverwritesPriorEntries) {
  TcpRemotingClient client(1, 1000, 1);
  client.updateNameServerAddressList("first:9876;second:9876");
  ASSERT_EQ((std::vector<std::string>{"first:9876", "second:9876"}), client.getNameServerAddressList());

  client.updateNameServerAddressList("third:10911");
  EXPECT_EQ((std::vector<std::string>{"third:10911"}), client.getNameServerAddressList());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "TcpRemotingClientTest.*";
  return RUN_ALL_TESTS();
}
