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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "MessageDecoder.h"
#include "MQException.h"
#include "MQMessage.h"
#include "MessageBatch.h"

using testing::InitGoogleMock;
using testing::InitGoogleTest;
using testing::Return;

using rocketmq::MessageBatch;
using rocketmq::MessageDecoder;
using rocketmq::MQClientException;
using rocketmq::MQMessage;
using rocketmq::stoba;

TEST(MessageBatchTest, Encode) {
  std::vector<MQMessage> msgs;
  msgs.push_back(MQMessage("topic", "*", "test1"));
  auto msgBatch = MessageBatch::generateFromList(msgs);
  auto encodeMessage = msgBatch->encode();
  auto encodeMessage2 = MessageDecoder::encodeMessages(msgs);
  EXPECT_EQ(encodeMessage, encodeMessage2);
  // 20 + bodyLen(test1) + 2 + propertiesLength(TAGS:*;WAIT:true;);
  EXPECT_EQ(encodeMessage.size(), 44);

  msgs.push_back(MQMessage("topic", "*", "test2"));
  msgs.push_back(MQMessage("topic", "*", "test3"));
  msgBatch = MessageBatch::generateFromList(msgs);
  encodeMessage = msgBatch->encode();
  encodeMessage2 = MessageDecoder::encodeMessages(msgs);
  EXPECT_EQ(encodeMessage, encodeMessage2);
  EXPECT_EQ(encodeMessage.size(), 132);  // 44 * 3
}

TEST(MessageBatchTest, RejectsRetryTopic) {
  std::vector<MQMessage> msgs;
  msgs.emplace_back("%RETRY%GID_group", "*", "body");
  EXPECT_THROW(MessageBatch::generateFromList(msgs), MQClientException);
}

TEST(MessageBatchTest, RejectsMixedTopics) {
  std::vector<MQMessage> msgs;
  msgs.emplace_back("TopicA", "*", "body1");
  msgs.emplace_back("TopicB", "*", "body2");
  EXPECT_THROW(MessageBatch::generateFromList(msgs), MQClientException);
}

TEST(MessageBatchTest, RejectsMixedWaitStoreFlags) {
  std::vector<MQMessage> msgs;
  MQMessage first("TopicA", "*", "body1");
  first.set_wait_store_msg_ok(true);
  MQMessage second("TopicA", "*", "body2");
  second.set_wait_store_msg_ok(false);
  msgs.push_back(first);
  msgs.push_back(second);
  EXPECT_THROW(MessageBatch::generateFromList(msgs), MQClientException);
}

TEST(MessageBatchTest, RejectsDelayedMessages) {
  std::vector<MQMessage> msgs;
  MQMessage delayed("TopicA", "*", "body");
  delayed.set_delay_time_level(2);
  msgs.push_back(delayed);
  EXPECT_THROW(MessageBatch::generateFromList(msgs), MQClientException);
}

int main(int argc, char* argv[]) {
  InitGoogleMock(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MessageBatchTest.*";
  return RUN_ALL_TESTS();
}
