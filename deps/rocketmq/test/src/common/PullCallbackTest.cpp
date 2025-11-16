#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "MQException.h"
#include "PullCallback.h"
#include "consumer/PullResult.h"

using rocketmq::AutoDeletePullCallback;
using rocketmq::MQException;
using rocketmq::PullCallback;
using rocketmq::PullResult;
using rocketmq::PullStatus;

class RecordingPullCallback : public PullCallback {
 public:
  void onSuccess(std::unique_ptr<PullResult> pull_result) override {
    success_called = true;
    last_status = pull_result->pull_status();
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
  PullStatus last_status{PullStatus::NO_NEW_MSG};
  std::string last_error;
};

class AutoDeletingPullCallback : public AutoDeletePullCallback {
 public:
  AutoDeletingPullCallback(bool* destroyed, bool* success_called, bool* exception_called)
      : destroyed_(destroyed), success_called_(success_called), exception_called_(exception_called) {}

  ~AutoDeletingPullCallback() override {
    if (destroyed_) {
      *destroyed_ = true;
    }
  }

  void onSuccess(std::unique_ptr<PullResult>) override {
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

TEST(PullCallbackTest, InvokeOnSuccessSwallowsHandlerExceptions) {
  RecordingPullCallback callback;
  callback.throw_in_success = true;

  auto result = std::unique_ptr<PullResult>(new PullResult(PullStatus::FOUND));
  EXPECT_NO_THROW(callback.invokeOnSuccess(std::move(result)));
  EXPECT_TRUE(callback.success_called);
  EXPECT_EQ(PullStatus::FOUND, callback.last_status);
}

TEST(PullCallbackTest, InvokeOnExceptionPassesExceptionObject) {
  RecordingPullCallback callback;
  MQException ex("failure", -1, __FILE__, __LINE__);

  callback.invokeOnException(ex);

  EXPECT_TRUE(callback.exception_called);
  EXPECT_EQ("failure", callback.last_error);
}

TEST(PullCallbackTest, AutoDeleteCallbackDeletesAfterSuccess) {
  bool destroyed = false;
  bool success_called = false;
  auto* callback = new AutoDeletingPullCallback(&destroyed, &success_called, nullptr);

  auto result = std::unique_ptr<PullResult>(new PullResult(PullStatus::FOUND));
  callback->invokeOnSuccess(std::move(result));

  EXPECT_TRUE(success_called);
  EXPECT_TRUE(destroyed);
}

TEST(PullCallbackTest, AutoDeleteCallbackDeletesAfterException) {
  bool destroyed = false;
  bool exception_called = false;
  auto* callback = new AutoDeletingPullCallback(&destroyed, nullptr, &exception_called);
  MQException ex("err", -2, __FILE__, __LINE__);

  callback->invokeOnException(ex);

  EXPECT_TRUE(exception_called);
  EXPECT_TRUE(destroyed);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "PullCallbackTest.*";
  return RUN_ALL_TESTS();
}
