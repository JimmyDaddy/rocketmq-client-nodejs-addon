#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "MQException.h"
#include "MQMessage.h"
#include "RequestCallback.h"
#include "producer/RequestResponseFuture.h"

using rocketmq::MQClientException;
using rocketmq::MQMessage;
using rocketmq::RequestCallback;
using rocketmq::RequestResponseFuture;

namespace {

class RecordingCallback : public RequestCallback {
 public:
  void onSuccess(MQMessage message) override {
    success_called = true;
    last_topic = message.topic();
  }

  void onException(rocketmq::MQException& e) noexcept override {
    exception_called = true;
    last_error = e.GetErrorMessage();
  }

  bool success_called = false;
  bool exception_called = false;
  std::string last_topic;
  std::string last_error;
};

}  // namespace

TEST(RequestResponseFutureTest, ExecuteRequestCallbackInvokesSuccessPath) {
  RecordingCallback callback;
  RequestResponseFuture future("corr-1", 5000, &callback);
  future.set_send_request_ok(true);

  auto response = std::make_shared<MQMessage>("ReplyTopic", "body");
  future.putResponseMessage(response);

  future.executeRequestCallback();

  EXPECT_TRUE(callback.success_called);
  EXPECT_FALSE(callback.exception_called);
  EXPECT_EQ("ReplyTopic", callback.last_topic);
}

TEST(RequestResponseFutureTest, ExecuteRequestCallbackInvokesExceptionPath) {
  RecordingCallback callback;
  RequestResponseFuture future("corr-2", 5000, &callback);
  future.set_send_request_ok(false);
  auto ex = MQClientException("failure", -1, __FILE__, __LINE__);
  future.set_cause(std::make_exception_ptr(ex));

  future.executeRequestCallback();

  EXPECT_TRUE(callback.exception_called);
  EXPECT_FALSE(callback.success_called);
  EXPECT_EQ("failure", callback.last_error);
}

TEST(RequestResponseFutureTest, WaitResponseMessageBlocksUntilResponseArrives) {
  RequestResponseFuture future("corr-3", 5000, nullptr);
  auto response = std::make_shared<MQMessage>("ReplyTopic", "body");

  std::thread responder([&future, response]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    future.putResponseMessage(response);
  });

  auto received = future.waitResponseMessage(1000);
  responder.join();

  EXPECT_EQ(response, received);
}

TEST(RequestResponseFutureTest, IsTimeoutReturnsTrueAfterDeadline) {
  RequestResponseFuture future("corr-4", 5, nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(future.isTimeout());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "RequestResponseFutureTest.*";
  return RUN_ALL_TESTS();
}
