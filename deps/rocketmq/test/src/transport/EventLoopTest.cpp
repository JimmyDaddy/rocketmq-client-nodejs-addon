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

#include <chrono>
#include <thread>

#include <event2/bufferevent.h>

#include "transport/EventLoop.h"

using rocketmq::BufferEvent;
using rocketmq::EventLoop;

TEST(EventLoopTest, StartStopLifecycleIsIdempotent) {
  EventLoop loop(nullptr, /*run_immediately=*/false);
  ASSERT_FALSE(loop.isRunning());

  loop.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(loop.isRunning());

  // repeated start should be harmless
  loop.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_TRUE(loop.isRunning());

  loop.stop();
  EXPECT_FALSE(loop.isRunning());

  // stopping twice should also be harmless
  loop.stop();
  EXPECT_FALSE(loop.isRunning());
}

TEST(EventLoopTest, BufferEventConnectRejectsInvalidAddresses) {
  EventLoop loop(nullptr, /*run_immediately=*/false);
  BufferEvent* buffer = loop.createBufferEvent(-1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
  ASSERT_NE(nullptr, buffer);

  EXPECT_EQ(-1, buffer->connect("definitely-not-a-host"));

  buffer->close();
  delete buffer;
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "EventLoopTest.*";
  return RUN_ALL_TESTS();
}
