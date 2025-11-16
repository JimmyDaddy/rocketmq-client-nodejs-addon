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

#include <map>
#include <memory>
#include <string>

#include "ClientRPCHook.h"
#include "CommandCustomHeader.h"
#include "RemotingCommand.h"
#include "SessionCredentials.h"
#include "protocol/RequestCode.h"
#include "protocol/header/CommandHeader.h"

using testing::InitGoogleMock;

using rocketmq::ClientRPCHook;
using rocketmq::MQRequestCode;
using rocketmq::RemotingCommand;
using rocketmq::SessionCredentials;

namespace {

class RecordingHeader : public rocketmq::CommandCustomHeader {
 public:
  explicit RecordingHeader(std::string declared) : declared_value_(std::move(declared)) {}

  void SetDeclaredFieldOfCommandHeader(std::map<std::string, std::string>& requestMap) override {
    requestMap["DeclaredKey"] = declared_value_;
    observed = requestMap;
  }

  std::string declared_value_;
  std::map<std::string, std::string> observed;
};

class SignatureSnapshot : public rocketmq::CommandCustomHeader {
 public:
  static SignatureSnapshot* Decode(std::map<std::string, std::string>& extFields) {
    auto* snapshot = new SignatureSnapshot();
    auto it = extFields.find("Signature");
    if (it != extFields.end()) {
      snapshot->signature = it->second;
    }
    it = extFields.find("AccessKey");
    if (it != extFields.end()) {
      snapshot->access_key = it->second;
    }
    it = extFields.find("OnsChannel");
    if (it != extFields.end()) {
      snapshot->ons_channel = it->second;
    }
    return snapshot;
  }

  std::string signature;
  std::string access_key;
  std::string ons_channel;
};

std::unique_ptr<RemotingCommand> CloneCommand(const RemotingCommand& command) {
  auto package = command.encode();
  return std::unique_ptr<RemotingCommand>(RemotingCommand::Decode(package, true));
}

std::string ExtractSignature(const RemotingCommand& command) {
  auto decoded = CloneCommand(command);
  auto* snapshot = decoded->decodeCommandCustomHeader<SignatureSnapshot>();
  return snapshot->signature;
}

}  // namespace

TEST(ClientRPCHookTest, HeaderFieldsInfluenceSignature) {
  SessionCredentials credentials("accessKey", "secretKey", "onsChannel");
  ClientRPCHook hook(credentials);

  auto* header = new RecordingHeader("DeclaredValueA");
  RemotingCommand command(MQRequestCode::UPDATE_AND_CREATE_TOPIC, header);
  command.set_body("payload");

  hook.doBeforeRequest("127.0.0.1:9876", command, true);

  auto* recorded = static_cast<RecordingHeader*>(command.readCustomHeader());
  ASSERT_NE(nullptr, recorded);
  ASSERT_FALSE(recorded->observed.empty());
  EXPECT_EQ("accessKey", recorded->observed["AccessKey"]);
  EXPECT_EQ("onsChannel", recorded->observed["OnsChannel"]);
  EXPECT_EQ(header->declared_value_, recorded->observed["DeclaredKey"]);

  auto signature_a = ExtractSignature(command);
  ASSERT_FALSE(signature_a.empty());

  auto* other_header = new RecordingHeader("DeclaredValueB");
  RemotingCommand other(MQRequestCode::UPDATE_AND_CREATE_TOPIC, other_header);
  other.set_body("payload");
  hook.doBeforeRequest("127.0.0.1:9876", other, true);

  EXPECT_NE(signature_a, ExtractSignature(other))
      << "Changing declared header content should change the signature";
}

TEST(ClientRPCHookTest, BodyAffectsSignatureAndSkipWhenNotSending) {
  SessionCredentials credentials("ak", "sk", "chan");
  ClientRPCHook hook(credentials);

  auto* header = new RecordingHeader("Declared");
  RemotingCommand first(MQRequestCode::SEND_MESSAGE, header);
  first.set_body("alpha");
  hook.doBeforeRequest("remote", first, true);

  auto* second_header = new RecordingHeader("Declared");
  RemotingCommand second(MQRequestCode::SEND_MESSAGE, second_header);
  second.set_body("beta");
  hook.doBeforeRequest("remote", second, true);

  EXPECT_NE(ExtractSignature(first), ExtractSignature(second))
      << "Different bodies should produce unique signatures";

  RemotingCommand unsent;
  hook.doBeforeRequest("remote", unsent, false);
  EXPECT_TRUE(ExtractSignature(unsent).empty());
}

TEST(ClientRPCHookTest, SignsResponsesOnlyWhenProvided) {
  SessionCredentials credentials("ak", "sk", "chan");
  ClientRPCHook hook(credentials);

  RemotingCommand request;
  RemotingCommand response(MQRequestCode::QUERY_BROKER_OFFSET, nullptr);
  response.set_body("replyBody");

  hook.doAfterResponse("remote", request, &response, true);
  auto signature_with_response = ExtractSignature(response);
  ASSERT_FALSE(signature_with_response.empty());

  RemotingCommand skipped;
  hook.doAfterResponse("remote", request, &skipped, false);
  EXPECT_TRUE(ExtractSignature(skipped).empty());
}

int main(int argc, char* argv[]) {
  InitGoogleMock(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ClientRPCHookTest.*";
  return RUN_ALL_TESTS();
}
