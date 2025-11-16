#include <gtest/gtest.h>

#include <stdexcept>

#include "MQException.h"
#include "SendCallback.h"
#include "SendResult.h"

using rocketmq::AutoDeleteSendCallback;
using rocketmq::MQException;
using rocketmq::SendCallback;
using rocketmq::SendResult;
using rocketmq::SendStatus;

class RecordingSendCallback : public SendCallback {
 public:
  void onSuccess(SendResult& send_result) override {
    success_called = true;
    last_status = send_result.send_status();
    if (throw_in_success) {
      throw std::runtime_error("boom");
    }
  }

  void onException(MQException& e) noexcept override {
    exception_called = true;
    last_error = e.GetErrorMessage();
  }

  bool success_called{false};
  bool exception_called{false};
  bool throw_in_success{false};
  SendStatus last_status{SendStatus::SEND_OK};
  std::string last_error;
};

class AutoDeletingSendCallback : public AutoDeleteSendCallback {
 public:
  AutoDeletingSendCallback(bool* destroyed, bool* success_called, bool* exception_called)
      : destroyed_(destroyed), success_called_(success_called), exception_called_(exception_called) {}

  ~AutoDeletingSendCallback() override {
    if (destroyed_) {
      *destroyed_ = true;
    }
  }

  void onSuccess(SendResult&) override {
    if (success_called_) {
      *success_called_ = true;
    }
  }

  void onException(MQException&) noexcept override {
    if (exception_called_) {
      *exception_called_ = true;
    }
  }

 private:
  bool* destroyed_;
  bool* success_called_;
  bool* exception_called_;
};

TEST(SendCallbackTest, InvokeOnSuccessSwallowsHandlerExceptions) {
  RecordingSendCallback callback;
  callback.throw_in_success = true;
  SendResult result(SendStatus::SEND_SLAVE_NOT_AVAILABLE, "mid", "offset", rocketmq::MQMessageQueue(), 7);

  EXPECT_NO_THROW(callback.invokeOnSuccess(result));
  EXPECT_TRUE(callback.success_called);
  EXPECT_EQ(SendStatus::SEND_SLAVE_NOT_AVAILABLE, callback.last_status);
}

TEST(SendCallbackTest, InvokeOnExceptionPassesContext) {
  RecordingSendCallback callback;
  MQException ex("failure", -1, __FILE__, __LINE__);

  callback.invokeOnException(ex);

  EXPECT_TRUE(callback.exception_called);
  EXPECT_EQ("failure", callback.last_error);
}

TEST(SendCallbackTest, AutoDeleteCallbackDeletesAfterSuccess) {
  bool destroyed = false;
  bool success_called = false;
  auto* callback = new AutoDeletingSendCallback(&destroyed, &success_called, nullptr);
  SendResult result;

  callback->invokeOnSuccess(result);

  EXPECT_TRUE(success_called);
  EXPECT_TRUE(destroyed);
}

TEST(SendCallbackTest, AutoDeleteCallbackDeletesAfterException) {
  bool destroyed = false;
  bool exception_called = false;
  auto* callback = new AutoDeletingSendCallback(&destroyed, nullptr, &exception_called);
  MQException ex("err", -2, __FILE__, __LINE__);

  callback->invokeOnException(ex);

  EXPECT_TRUE(exception_called);
  EXPECT_TRUE(destroyed);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "SendCallbackTest.*";
  return RUN_ALL_TESTS();
}
