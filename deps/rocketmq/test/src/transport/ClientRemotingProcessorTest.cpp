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
#include "ClientRemotingProcessor.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>

#include "ByteArray.h"
#include "MQException.h"
#include "MQMessageConst.h"
#include "MessageDecoder.h"
#include "RemotingCommand.h"
#include "RequestCallback.h"
#include "RequestFutureTable.h"
#include "RequestResponseFuture.h"
#include "UtilAll.h"
#include "MessageSysFlag.h"
#include "protocol/RequestCode.h"
#include "protocol/ResponseCode.h"
#include "protocol/header/ReplyMessageRequestHeader.hpp"

using rocketmq::AutoDeleteRequestCallback;
using rocketmq::ClientRemotingProcessor;
using rocketmq::MQException;
using rocketmq::MQMessage;
using rocketmq::MQMessageConst;
using rocketmq::MessageDecoder;
using rocketmq::RemotingCommand;
using rocketmq::RequestCallback;
using rocketmq::RequestFutureTable;
using rocketmq::RequestResponseFuture;
using rocketmq::ReplyMessageRequestHeader;
using rocketmq::UtilAll;

namespace {

std::map<std::string, std::string> BasicProperties(const std::string& correlation_id) {
  std::map<std::string, std::string> properties;
  properties[MQMessageConst::PROPERTY_CORRELATION_ID] = correlation_id;
  properties[MQMessageConst::PROPERTY_MESSAGE_REPLY_TO_CLIENT] = "clientId";
  return properties;
}

ReplyMessageRequestHeader* BuildHeader(const std::string& correlation_id, bool compressed) {
  auto* header = new ReplyMessageRequestHeader();
  header->set_producer_group("group");
  header->set_topic("ReplyTopic");
  header->set_default_topic("Default");
  header->set_default_topic_queue_nums(4);
  header->set_queue_id(1);
  header->set_sys_flag(compressed ? rocketmq::MessageSysFlag::COMPRESSED_FLAG : 0);
  header->set_born_timestamp(123);
  header->set_flag(0);
  header->set_reconsume_times(0);
  header->set_unit_mode(false);
  header->set_born_host("127.0.0.1:10909");
  header->set_store_host("127.0.0.1:10911");
  header->set_store_timestamp(456);
  header->set_properties(MessageDecoder::messageProperties2String(BasicProperties(correlation_id)));
  return header;
}

class RecordingRequestCallback : public RequestCallback {
 public:
  void onSuccess(MQMessage message) override {
    invoked = true;
    last_body = message.body();
    last_topic = message.topic();
  }

  void onException(MQException& e) noexcept override {
    invoked = true;
    exception_message = e.what();
  }

  bool invoked{false};
  std::string last_body;
  std::string last_topic;
  std::string exception_message;
};

std::unique_ptr<RemotingCommand> MakeReplyCommand(const std::string& correlation_id,
                                                  bool compressed,
                                                  const std::string& body) {
  auto* header = BuildHeader(correlation_id, compressed);
  std::unique_ptr<RemotingCommand> command(
      new RemotingCommand(rocketmq::PUSH_REPLY_MESSAGE_TO_CLIENT, header));
  if (!body.empty()) {
    if (compressed) {
      std::string compressed_body;
      UtilAll::deflate(body, compressed_body, 5);
      command->set_body(rocketmq::stoba(std::move(compressed_body)));
    } else {
      command->set_body(rocketmq::stoba(body));
    }
  }
  return command;
}

}  // namespace

TEST(ClientRemotingProcessorTest, ReceiveReplyMessageFailsWhenBodyMissing) {
  ClientRemotingProcessor processor(nullptr);
  const std::string correlation_id = "corr-missing";

  RecordingRequestCallback callback;
  auto future = std::make_shared<RequestResponseFuture>(correlation_id, 3000, &callback);
  future->set_send_request_ok(true);
  RequestFutureTable::putRequestFuture(correlation_id, future);

  auto command = MakeReplyCommand(correlation_id, false, "");
  std::unique_ptr<RemotingCommand> response(processor.receiveReplyMessage(command.get()));

  ASSERT_NE(nullptr, response);
  EXPECT_EQ(rocketmq::SYSTEM_ERROR, response->code());
  EXPECT_EQ("reply message body is empty", response->remark());

  EXPECT_FALSE(future->send_request_ok());
  EXPECT_NE(nullptr, future->cause());
  auto removed = RequestFutureTable::removeRequestFuture(correlation_id);
  EXPECT_EQ(nullptr, removed);
}

TEST(ClientRemotingProcessorTest, ReceiveReplyMessageDeliversDecompressedBody) {
  ClientRemotingProcessor processor(nullptr);
  const std::string correlation_id = "corr-success";

  RecordingRequestCallback callback;
  auto future = std::make_shared<RequestResponseFuture>(correlation_id, 3000, &callback);
  future->set_send_request_ok(true);
  RequestFutureTable::putRequestFuture(correlation_id, future);

  const std::string body = "reply-payload";
  auto command = MakeReplyCommand(correlation_id, true, body);

  std::unique_ptr<RemotingCommand> response(processor.receiveReplyMessage(command.get()));
  ASSERT_NE(nullptr, response);
  EXPECT_EQ(rocketmq::SUCCESS, response->code());
  EXPECT_TRUE(response->remark().empty());

  EXPECT_TRUE(callback.invoked);
  EXPECT_EQ(body, callback.last_body);
  EXPECT_EQ("ReplyTopic", callback.last_topic);
  EXPECT_TRUE(callback.exception_message.empty());

  EXPECT_EQ(nullptr, RequestFutureTable::removeRequestFuture(correlation_id));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ClientRemotingProcessorTest.*";
  return RUN_ALL_TESTS();
}
