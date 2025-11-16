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
#include <string>
#include <vector>

#include "ByteArray.h"
#include "MQClientAPIImpl.h"
#include "MQClientConfigImpl.hpp"
#include "MQException.h"
#include "MQMessage.h"
#include "MQMessageConst.h"
#include "MQProtos.h"
#include "Message.h"
#include "MessageBatch.h"
#include "MessageImpl.h"
#include "PullResult.h"
#include "PullResultExt.hpp"
#include "RemotingCommand.h"
#include "protocol/header/CommandHeader.h"

using rocketmq::ByteArrayRef;
using rocketmq::CommunicationMode;
using rocketmq::MQBrokerException;
using rocketmq::MQClientAPIImpl;
using rocketmq::MQClientConfigImpl;
using rocketmq::MQMessage;
using rocketmq::MQMessageConst;
using rocketmq::Message;
using rocketmq::MessageBatch;
using rocketmq::MessageImpl;
using rocketmq::MessagePtr;
using rocketmq::PullMessageResponseHeader;
using rocketmq::PullResult;
using rocketmq::PullResultExt;
using rocketmq::PullStatus;
using rocketmq::RemotingCommand;
using rocketmq::SendMessageResponseHeader;
using rocketmq::SendResult;
using rocketmq::SendStatus;
using rocketmq::stoba;

namespace {

MQClientConfigImpl MakeClientConfig() {
  MQClientConfigImpl config;
  config.set_tcp_transport_worker_thread_nums(2);
  config.set_tcp_transport_connect_timeout(500);
  config.set_tcp_transport_try_lock_timeout(2000);
  return config;
}

MessagePtr MakeMessage(const std::string& topic, const std::string& uniq_id) {
  auto message = std::make_shared<MessageImpl>();
  message->set_topic(topic);
  message->set_body("payload");
  message->putProperty(MQMessageConst::PROPERTY_UNIQ_CLIENT_MESSAGE_ID_KEYIDX, uniq_id);
  return message;
}

std::unique_ptr<SendResult> InvokeProcessSendResponse(MQClientAPIImpl& api,
                                                      int response_code,
                                                      const MessagePtr& msg,
                                                      const std::string& broker_name,
                                                      int queue_id = 3) {
  auto* header = new SendMessageResponseHeader();
  header->queueId = queue_id;
  header->queueOffset = 1234;
  header->msgId = "REMOTE-ID";
  header->transactionId = "TX-42";
  RemotingCommand response(response_code, header);
  return std::unique_ptr<SendResult>(api.processSendResponse(broker_name, msg, &response));
}

class MQClientAPIImplTest : public ::testing::Test {
 protected:
  MQClientAPIImplTest() : config_(MakeClientConfig()), api_(nullptr, nullptr, config_) {}

  MQClientConfigImpl config_;
  MQClientAPIImpl api_;
};

}  // namespace

TEST_F(MQClientAPIImplTest, ProcessSendResponseMapsBrokerCodesToStatuses) {
  struct {
    int response_code;
    SendStatus expected_status;
  } cases[] = {
      {rocketmq::SUCCESS, rocketmq::SEND_OK},
      {rocketmq::FLUSH_DISK_TIMEOUT, rocketmq::SEND_FLUSH_DISK_TIMEOUT},
      {rocketmq::FLUSH_SLAVE_TIMEOUT, rocketmq::SEND_FLUSH_SLAVE_TIMEOUT},
      {rocketmq::SLAVE_NOT_AVAILABLE, rocketmq::SEND_SLAVE_NOT_AVAILABLE},
  };

  auto message = MakeMessage("TopicTest", "UNIQ-1");
  for (const auto& testCase : cases) {
    auto result = InvokeProcessSendResponse(api_, testCase.response_code, message, "brokerA", 2);
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(testCase.expected_status, result->send_status());
    EXPECT_EQ("UNIQ-1", result->msg_id());
    EXPECT_EQ("REMOTE-ID", result->offset_msg_id());
    EXPECT_EQ("brokerA", result->message_queue().broker_name());
    EXPECT_EQ(2, result->message_queue().queue_id());
    EXPECT_EQ(1234, result->queue_offset());
    EXPECT_EQ("TX-42", result->transaction_id());
  }
}

TEST_F(MQClientAPIImplTest, ProcessSendResponseConcatenatesBatchUniqIds) {
  std::vector<MQMessage> batch_messages;
  MQMessage first("TopicBatch", "*", "body-1");
  first.putProperty(MQMessageConst::PROPERTY_UNIQ_CLIENT_MESSAGE_ID_KEYIDX, "A");
  MQMessage second("TopicBatch", "*", "body-2");
  second.putProperty(MQMessageConst::PROPERTY_UNIQ_CLIENT_MESSAGE_ID_KEYIDX, "B");
  batch_messages.push_back(first);
  batch_messages.push_back(second);

  auto batch = MessageBatch::generateFromList(batch_messages);
  MessagePtr message = std::static_pointer_cast<Message>(batch);

  auto result = InvokeProcessSendResponse(api_, rocketmq::SUCCESS, message, "brokerB", 9);
  ASSERT_NE(nullptr, result);
  EXPECT_EQ("A,B", result->msg_id());
  EXPECT_EQ("brokerB", result->message_queue().broker_name());
  EXPECT_EQ(9, result->message_queue().queue_id());
}

TEST_F(MQClientAPIImplTest, ProcessSendResponseThrowsForUnexpectedCodes) {
  auto message = MakeMessage("TopicTest", "UNIQ-ERR");
  auto* header = new SendMessageResponseHeader();
  header->queueId = 1;
  RemotingCommand response(rocketmq::SYSTEM_ERROR, header);
  response.set_remark("boom");
  EXPECT_THROW({ api_.processSendResponse("brokerC", message, &response); }, MQBrokerException);
}

TEST_F(MQClientAPIImplTest, ProcessPullResponseExposesMetadataAndBinaryBody) {
  auto* header = new PullMessageResponseHeader();
  header->nextBeginOffset = 101;
  header->minOffset = 11;
  header->maxOffset = 999;
  header->suggestWhichBrokerId = 3;
  RemotingCommand response(rocketmq::SUCCESS, header);
  response.set_body(stoba("binary-body"));

  std::unique_ptr<PullResult> result(api_.processPullResponse(&response));
  ASSERT_NE(nullptr, result);
  EXPECT_EQ(rocketmq::FOUND, result->pull_status());
  EXPECT_EQ(101, result->next_begin_offset());
  EXPECT_EQ(11, result->min_offset());
  EXPECT_EQ(999, result->max_offset());

  auto* ext = dynamic_cast<PullResultExt*>(result.get());
  ASSERT_NE(nullptr, ext);
  EXPECT_EQ(3, ext->suggert_which_boker_id());
  ASSERT_NE(nullptr, ext->message_binary());
  EXPECT_EQ("binary-body", rocketmq::batos(ext->message_binary()));
}

TEST_F(MQClientAPIImplTest, ProcessPullResponseDifferentiatesRetryRemarks) {
  auto* overflow_header = new PullMessageResponseHeader();
  RemotingCommand overflow_response(rocketmq::PULL_RETRY_IMMEDIATELY, overflow_header);
  overflow_response.set_remark("OFFSET_OVERFLOW_BADLY");
  std::unique_ptr<PullResult> overflow(api_.processPullResponse(&overflow_response));
  ASSERT_NE(nullptr, overflow);
  EXPECT_EQ(rocketmq::NO_LATEST_MSG, overflow->pull_status());

  auto* filtered_header = new PullMessageResponseHeader();
  RemotingCommand filtered_response(rocketmq::PULL_RETRY_IMMEDIATELY, filtered_header);
  filtered_response.set_remark("TAG_MISS");
  std::unique_ptr<PullResult> filtered(api_.processPullResponse(&filtered_response));
  ASSERT_NE(nullptr, filtered);
  EXPECT_EQ(rocketmq::NO_MATCHED_MSG, filtered->pull_status());
}

TEST_F(MQClientAPIImplTest, ProcessPullResponseThrowsForUnknownCodes) {
  auto* header = new PullMessageResponseHeader();
  RemotingCommand response(rocketmq::SYSTEM_ERROR, header);
  EXPECT_THROW(api_.processPullResponse(&response), MQBrokerException);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MQClientAPIImplTest.*";
  return RUN_ALL_TESTS();
}
