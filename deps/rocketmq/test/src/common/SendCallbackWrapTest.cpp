#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "MQException.h"
#include "MQMessage.h"
#include "ResponseFuture.h"
#include "common/SendCallbackWrap.h"
#include "producer/DefaultMQProducerConfigImpl.hpp"
#include "producer/DefaultMQProducerImpl.h"
#include "protocol/RequestCode.h"
#include "protocol/header/CommandHeader.h"

using rocketmq::DefaultMQProducerConfigImpl;
using rocketmq::DefaultMQProducerImpl;
using rocketmq::DefaultMQProducerImplPtr;
using rocketmq::MQException;
using rocketmq::MQMessage;
using rocketmq::MessagePtr;
using rocketmq::MQRequestCode;
using rocketmq::RemotingCommand;
using rocketmq::ResponseFuture;
using rocketmq::SendCallback;
using rocketmq::SendCallbackWrap;
using rocketmq::SendMessageRequestHeader;

namespace {

class RecordingSendCallback : public SendCallback {
 public:
  void onSuccess(rocketmq::SendResult& send_result) override {
    success_called = true;
    last_status = send_result.send_status();
  }

  void onException(MQException& e) noexcept override {
    exception_called = true;
    last_error = e.GetErrorMessage();
  }

  bool success_called{false};
  bool exception_called{false};
  rocketmq::SendStatus last_status{rocketmq::SendStatus::SEND_OK};
  std::string last_error;
};

DefaultMQProducerImplPtr MakeProducer(const std::string& group) {
  auto config = std::make_shared<DefaultMQProducerConfigImpl>();
  config->set_group_name(group);
  config->set_instance_name(group + "Instance");
  config->set_namesrv_addr("127.0.0.1:9876");
  return DefaultMQProducerImpl::create(config);
}

MessagePtr MakeMessage() {
  MQMessage message("SendCallbackWrapTestTopic", "payload");
  return message.getMessageImpl();
}

RemotingCommand MakeSendRequest() {
  auto* header = new SendMessageRequestHeader();
  header->queueId = 0;
  return RemotingCommand(MQRequestCode::SEND_MESSAGE, header);
}

}  // namespace

TEST(SendCallbackWrapTest, ProducerReleasedInvokesExceptionCallback) {
  RecordingSendCallback callback;
  auto message = MakeMessage();
  auto request = MakeSendRequest();
  SendCallbackWrap wrap("127.0.0.1:10911", "brokerA", message, std::move(request), &callback, nullptr, nullptr, 2,
                       0, nullptr);
  ResponseFuture future(MQRequestCode::SEND_MESSAGE, 1, 1000);

  wrap.operationComplete(&future);

  EXPECT_TRUE(callback.exception_called);
  EXPECT_EQ("DefaultMQProducer is released.", callback.last_error);
}

TEST(SendCallbackWrapTest, OnExceptionWithoutRetryNotifiesCallback) {
  auto producer = MakeProducer("SendCallbackWrapNoRetry");
  RecordingSendCallback callback;
  auto message = MakeMessage();
  auto request = MakeSendRequest();
  SendCallbackWrap wrap("127.0.0.1:10911", "brokerB", message, std::move(request), &callback, nullptr, nullptr, 0,
                       0, producer);
  ResponseFuture future(MQRequestCode::SEND_MESSAGE, 2, 1000);
  MQException error("explicit failure", -1, __FILE__, __LINE__);

  wrap.onExceptionImpl(&future, 100, error, false);

  EXPECT_TRUE(callback.exception_called);
  EXPECT_EQ("explicit failure", callback.last_error);
}

TEST(SendCallbackWrapTest, OperationCompleteWithoutResponseFallsBackToCallback) {
  auto producer = MakeProducer("SendCallbackWrapNoResponse");
  RecordingSendCallback callback;
  auto message = MakeMessage();
  auto request = MakeSendRequest();
  SendCallbackWrap wrap("127.0.0.1:10911", "brokerC", message, std::move(request), &callback, nullptr, nullptr, 0,
                       0, producer);
  ResponseFuture future(MQRequestCode::SEND_MESSAGE, 3, 1000);
  future.set_send_request_ok(false);

  wrap.operationComplete(&future);

  EXPECT_TRUE(callback.exception_called);
  EXPECT_EQ("send request failed", callback.last_error);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "SendCallbackWrapTest.*";
  return RUN_ALL_TESTS();
}
