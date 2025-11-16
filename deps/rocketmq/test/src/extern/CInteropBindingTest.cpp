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

#include <cstdio>
#include <cstring>

#include "CErrorContainer.h"
#include "SendResult.h"
#include "c/CErrorMessage.h"
#include "c/CSendResult.h"

using rocketmq::CErrorContainer;
using rocketmq::SendStatus;

TEST(CInteropBindingTest, GetLatestErrorMessageReflectsContainerState) {
  CErrorContainer::setErrorMessage("first-error");
  const char* first = GetLatestErrorMessage();
  ASSERT_STREQ("first-error", first);

  std::string dynamic_error("dynamic-error");
  CErrorContainer::setErrorMessage(dynamic_error);
  const char* second = GetLatestErrorMessage();
  EXPECT_STREQ(dynamic_error.c_str(), second);
}

TEST(CInteropBindingTest, CSendStatusMatchesCppEnumValues) {
  EXPECT_EQ(static_cast<int>(SendStatus::SEND_OK), static_cast<int>(E_SEND_OK));
  EXPECT_EQ(static_cast<int>(SendStatus::SEND_FLUSH_DISK_TIMEOUT),
            static_cast<int>(E_SEND_FLUSH_DISK_TIMEOUT));
  EXPECT_EQ(static_cast<int>(SendStatus::SEND_FLUSH_SLAVE_TIMEOUT),
            static_cast<int>(E_SEND_FLUSH_SLAVE_TIMEOUT));
  EXPECT_EQ(static_cast<int>(SendStatus::SEND_SLAVE_NOT_AVAILABLE),
            static_cast<int>(E_SEND_SLAVE_NOT_AVAILABLE));
}

TEST(CInteropBindingTest, CSendResultStoresStatusMessageIdAndOffset) {
  CSendResult result{};
  result.sendStatus = E_SEND_OK;
  std::snprintf(result.msgId, sizeof(result.msgId), "%s", "00000000000000000000000000000001");
  result.offset = 42;

  EXPECT_EQ(E_SEND_OK, result.sendStatus);
  EXPECT_STREQ("00000000000000000000000000000001", result.msgId);
  EXPECT_EQ(42, result.offset);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "CInteropBindingTest.*";
  return RUN_ALL_TESTS();
}
