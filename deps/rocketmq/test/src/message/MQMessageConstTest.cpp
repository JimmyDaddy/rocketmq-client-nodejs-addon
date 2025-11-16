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

#include <array>
#include <string>

#include "MQMessageConst.h"

using rocketmq::MQMessageConst;

TEST(MQMessageConstTest, ExposesAllExpectedPropertyKeys) {
  struct Entry {
    const std::string& actual;
    const char* expected;
  };

  const std::array<Entry, 28> entries = {{{MQMessageConst::PROPERTY_KEYS, "KEYS"},
                                          {MQMessageConst::PROPERTY_TAGS, "TAGS"},
                                          {MQMessageConst::PROPERTY_WAIT_STORE_MSG_OK, "WAIT"},
                                          {MQMessageConst::PROPERTY_DELAY_TIME_LEVEL, "DELAY"},
                                          {MQMessageConst::PROPERTY_RETRY_TOPIC, "RETRY_TOPIC"},
                                          {MQMessageConst::PROPERTY_REAL_TOPIC, "REAL_TOPIC"},
                                          {MQMessageConst::PROPERTY_REAL_QUEUE_ID, "REAL_QID"},
                                          {MQMessageConst::PROPERTY_TRANSACTION_PREPARED, "TRAN_MSG"},
                                          {MQMessageConst::PROPERTY_PRODUCER_GROUP, "PGROUP"},
                                          {MQMessageConst::PROPERTY_MIN_OFFSET, "MIN_OFFSET"},
                                          {MQMessageConst::PROPERTY_MAX_OFFSET, "MAX_OFFSET"},
                                          {MQMessageConst::PROPERTY_BUYER_ID, "BUYER_ID"},
                                          {MQMessageConst::PROPERTY_ORIGIN_MESSAGE_ID, "ORIGIN_MESSAGE_ID"},
                                          {MQMessageConst::PROPERTY_TRANSFER_FLAG, "TRANSFER_FLAG"},
                                          {MQMessageConst::PROPERTY_CORRECTION_FLAG, "CORRECTION_FLAG"},
                                          {MQMessageConst::PROPERTY_MQ2_FLAG, "MQ2_FLAG"},
                                          {MQMessageConst::PROPERTY_RECONSUME_TIME, "RECONSUME_TIME"},
                                          {MQMessageConst::PROPERTY_MSG_REGION, "MSG_REGION"},
                                          {MQMessageConst::PROPERTY_TRACE_SWITCH, "TRACE_ON"},
                                          {MQMessageConst::PROPERTY_UNIQ_CLIENT_MESSAGE_ID_KEYIDX, "UNIQ_KEY"},
                                          {MQMessageConst::PROPERTY_MAX_RECONSUME_TIMES, "MAX_RECONSUME_TIMES"},
                                          {MQMessageConst::PROPERTY_CONSUME_START_TIMESTAMP, "CONSUME_START_TIME"},
                                          {MQMessageConst::PROPERTY_TRANSACTION_PREPARED_QUEUE_OFFSET,
                                           "TRAN_PREPARED_QUEUE_OFFSET"},
                                          {MQMessageConst::PROPERTY_TRANSACTION_CHECK_TIMES, "TRANSACTION_CHECK_TIMES"},
                                          {MQMessageConst::PROPERTY_CHECK_IMMUNITY_TIME_IN_SECONDS,
                                           "CHECK_IMMUNITY_TIME_IN_SECONDS"},
                                          {MQMessageConst::PROPERTY_INSTANCE_ID, "INSTANCE_ID"},
                                          {MQMessageConst::PROPERTY_CORRELATION_ID, "CORRELATION_ID"},
                                          {MQMessageConst::PROPERTY_MESSAGE_REPLY_TO_CLIENT, "REPLY_TO_CLIENT"}}};

  for (const auto& entry : entries) {
    EXPECT_EQ(entry.expected, entry.actual);
  }
}

TEST(MQMessageConstTest, ProvidesRemainingMessagePropertyNames) {
  EXPECT_EQ("TTL", MQMessageConst::PROPERTY_MESSAGE_TTL);
  EXPECT_EQ("ARRIVE_TIME", MQMessageConst::PROPERTY_REPLY_MESSAGE_ARRIVE_TIME);
  EXPECT_EQ("PUSH_REPLY_TIME", MQMessageConst::PROPERTY_PUSH_REPLY_TIME);
  EXPECT_EQ("CLUSTER", MQMessageConst::PROPERTY_CLUSTER);
  EXPECT_EQ("MSG_TYPE", MQMessageConst::PROPERTY_MESSAGE_TYPE);
  EXPECT_EQ("__ALREADY_COMPRESSED__", MQMessageConst::PROPERTY_ALREADY_COMPRESSED_FLAG);
  EXPECT_EQ(" ", MQMessageConst::KEY_SEPARATOR);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQMessageConstTest.*";
  return RUN_ALL_TESTS();
}
