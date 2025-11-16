#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "MQClientAPIImpl.h"
#include "MQException.h"
#include "PullCallback.h"
#include "ResponseFuture.h"
#include "RemotingCommand.h"
#include "common/PullCallbackWrap.h"
#include "consumer/PullResult.h"
#include "producer/DefaultMQProducerConfigImpl.hpp"
#include "protocol/RequestCode.h"
#include "protocol/ResponseCode.h"
#include "protocol/header/CommandHeader.h"

using rocketmq::DefaultMQProducerConfigImpl;
using rocketmq::MQClientAPIImpl;
using rocketmq::MQException;
using rocketmq::MQRequestCode;
using rocketmq::PullCallback;
using rocketmq::PullCallbackWrap;
using rocketmq::PullResult;
using rocketmq::PullStatus;
using rocketmq::RemotingCommand;
using rocketmq::ResponseFuture;

namespace {

class RecordingPullCallback : public PullCallback {
 public:
  void onSuccess(std::unique_ptr<PullResult> pull_result) override {
    success_called = true;
    last_status = pull_result->pull_status();
  }

  void onException(MQException& e) noexcept override {
    exception_called = true;
    last_error = e.GetErrorMessage();
  }

  bool success_called{false};
  bool exception_called{false};
  PullStatus last_status{PullStatus::NO_NEW_MSG};
  std::string last_error;
};

std::unique_ptr<MQClientAPIImpl> MakeClientAPI() {
  DefaultMQProducerConfigImpl config;
  config.set_group_name("PullCallbackWrapTestGroup");
  config.set_namesrv_addr("127.0.0.1:9876");
  config.set_instance_name("PullCallbackWrapTester");
  return std::unique_ptr<MQClientAPIImpl>(new MQClientAPIImpl(nullptr, nullptr, config));
}

std::unique_ptr<RemotingCommand> MakePullResponse(int32_t code, const std::string& remark = std::string()) {
  auto* header = new rocketmq::PullMessageResponseHeader();
  header->nextBeginOffset = 11;
  header->minOffset = 1;
  header->maxOffset = 99;
  header->suggestWhichBrokerId = 0;
  auto response = std::unique_ptr<RemotingCommand>(new RemotingCommand(code, header));
  if (!remark.empty()) {
    response->set_remark(remark);
  }
  return response;
}

}  // namespace

TEST(PullCallbackWrapTest, ForwardsSuccessfulPullResultsToCallback) {
  auto client_api = MakeClientAPI();
  RecordingPullCallback callback;
  PullCallbackWrap wrap(&callback, client_api.get());
  ResponseFuture future(MQRequestCode::PULL_MESSAGE, 11, 1000);
  future.setResponseCommand(MakePullResponse(rocketmq::SUCCESS));

  wrap.operationComplete(&future);

  EXPECT_TRUE(callback.success_called);
  EXPECT_EQ(PullStatus::FOUND, callback.last_status);
  EXPECT_FALSE(callback.exception_called);
}

TEST(PullCallbackWrapTest, PropagatesProcessPullResponseExceptions) {
  auto client_api = MakeClientAPI();
  RecordingPullCallback callback;
  PullCallbackWrap wrap(&callback, client_api.get());
  ResponseFuture future(MQRequestCode::PULL_MESSAGE, 12, 1000);
  future.setResponseCommand(MakePullResponse(12345, "broker failure"));

  wrap.operationComplete(&future);

  EXPECT_TRUE(callback.exception_called);
  EXPECT_EQ("broker failure", callback.last_error);
}

TEST(PullCallbackWrapTest, ReportsTransportErrorsWhenNoResponseArrives) {
  auto client_api = MakeClientAPI();
  RecordingPullCallback callback;
  PullCallbackWrap wrap(&callback, client_api.get());
  ResponseFuture future(MQRequestCode::PULL_MESSAGE, 13, 1000);
  future.set_send_request_ok(false);

  wrap.operationComplete(&future);

  EXPECT_TRUE(callback.exception_called);
  EXPECT_EQ("send request failed", callback.last_error);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "PullCallbackWrapTest.*";
  return RUN_ALL_TESTS();
}
