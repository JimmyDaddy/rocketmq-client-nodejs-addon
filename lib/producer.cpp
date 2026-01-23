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
#include "producer.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <string>

#include <napi.h>

#include <ClientRPCHook.h>
#include <LoggerConfig.h>
#include <MQException.h>
#include <MQMessage.h>
#include <SendCallback.h>

#include "addon_data.h"

namespace __node_rocketmq__ {

#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
namespace {
bool IsEnvEnabled(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr) {
    return false;
  }
  return value[0] != '\0' && value[0] != '0';
}
}
#endif

Napi::Object RocketMQProducer::Init(Napi::Env env, Napi::Object exports, AddonData* addon_data) {
  Napi::Function func =
      DefineClass(env,
                  "RocketMQProducer",
                  {
                      InstanceMethod<&RocketMQProducer::Start>("start"),
                      InstanceMethod<&RocketMQProducer::Shutdown>("shutdown"),
                      InstanceMethod<&RocketMQProducer::Send>("send"),
                      InstanceMethod<&RocketMQProducer::SetSessionCredentials>(
                          "setSessionCredentials"),
                  });

  addon_data->producer_constructor = Napi::Persistent(func);

  exports.Set("Producer", func);
  return exports;
}

RocketMQProducer::RocketMQProducer(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<RocketMQProducer>(info), producer_("") {
  const Napi::Value group_name = info[0];
  if (group_name.IsString()) {
    producer_.set_group_name(group_name.ToString());
  }

  const Napi::Value instance_name = info[1];
  if (instance_name.IsString()) {
    producer_.set_instance_name(instance_name.ToString());
  }

  const Napi::Value options = info[2];
  if (options.IsObject()) {
    // try to set options
    SetOptions(options.ToObject());
  }
}

RocketMQProducer::~RocketMQProducer() {
  producer_.shutdown();
}

void RocketMQProducer::SetOptions(const Napi::Object& options) {
  // set name server
  Napi::Value name_server = options.Get("nameServer");
  if (name_server.IsString()) {
    producer_.set_namesrv_addr(name_server.ToString());
  }

  // set group name
  Napi::Value group_name = options.Get("groupName");
  if (group_name.IsString()) {
    producer_.set_group_name(group_name.ToString());
  }

  // set max message size
  Napi::Value max_message_size = options.Get("maxMessageSize");
  if (max_message_size.IsNumber()) {
    producer_.set_max_message_size(max_message_size.ToNumber());
  }

  // set compress level
  Napi::Value compress_level = options.Get("compressLevel");
  if (compress_level.IsNumber()) {
    producer_.set_compress_level(compress_level.ToNumber());
  }

  // set send message timeout
  Napi::Value send_message_timeout = options.Get("sendMessageTimeout");
  if (send_message_timeout.IsNumber()) {
    producer_.set_send_msg_timeout(send_message_timeout.ToNumber());
  }

  // set log level
  Napi::Value log_level = options.Get("logLevel");
  if (log_level.IsNumber()) {
    int32_t level = log_level.ToNumber();
    if (level >= 0 && level < rocketmq::LogLevel::LOG_LEVEL_LEVEL_NUM) {
      rocketmq::GetDefaultLoggerConfig().set_level(
          static_cast<rocketmq::LogLevel>(level));
    }
  }

  // set log directory
  Napi::Value log_dir = options.Get("logDir");
  if (log_dir.IsString()) {
    rocketmq::GetDefaultLoggerConfig().set_path(log_dir.ToString());
  }

  // set log file size
  Napi::Value log_file_size = options.Get("logFileSize");
  if (log_file_size.IsNumber()) {
    rocketmq::GetDefaultLoggerConfig().set_file_size(log_file_size.ToNumber());
  }

  // set log file num
  Napi::Value log_file_num = options.Get("logFileNum");
  if (log_file_num.IsNumber()) {
    rocketmq::GetDefaultLoggerConfig().set_file_count(log_file_num.ToNumber());
  }
}

Napi::Value RocketMQProducer::SetSessionCredentials(
    const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if required parameters are provided
  if (info.Length() < 3) {
    Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (!info[0].IsString() || !info[1].IsString() || !info[2].IsString()) {
    Napi::TypeError::New(env, "All arguments must be strings").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::String access_key = info[0].As<Napi::String>();
  Napi::String secret_key = info[1].As<Napi::String>();
  Napi::String ons_channel = info[2].As<Napi::String>();

  auto rpc_hook = std::make_shared<rocketmq::ClientRPCHook>(
      rocketmq::SessionCredentials(access_key, secret_key, ons_channel));
  producer_.setRPCHook(rpc_hook);

  return env.Undefined();
}

class ProducerStartWorker : public Napi::AsyncWorker {
 public:
  ProducerStartWorker(const Napi::Function& callback,
                      RocketMQProducer* wrapper)
      : Napi::AsyncWorker(callback),
        wrapper_ref_(Napi::Persistent(wrapper->Value())),
        producer_(&wrapper->producer_) {}

  void Execute() override {
    try {
      producer_->start();
    } catch (const std::exception& e) {
      SetError(e.what());
    }
  }

 private:
  Napi::ObjectReference wrapper_ref_;
  rocketmq::DefaultMQProducer* producer_;
};

Napi::Value RocketMQProducer::Start(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if callback is provided and is a function
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Function expected as first argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  auto* worker = new ProducerStartWorker(callback, this);
  worker->Queue();
  return env.Undefined();
}

class ProducerShutdownWorker : public Napi::AsyncWorker {
 public:
  ProducerShutdownWorker(const Napi::Function& callback,
                         RocketMQProducer* wrapper)
      : Napi::AsyncWorker(callback),
        wrapper_ref_(Napi::Persistent(wrapper->Value())),
        producer_(&wrapper->producer_) {}

  void Execute() override {
    try {
      producer_->shutdown();
    } catch (const std::exception& e) {
      SetError(e.what());
    }
  }

 private:
  Napi::ObjectReference wrapper_ref_;
  rocketmq::DefaultMQProducer* producer_;
};

Napi::Value RocketMQProducer::Shutdown(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if callback is provided and is a function
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Function expected as first argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  auto* worker = new ProducerShutdownWorker(callback, this);
  worker->Queue();
  return env.Undefined();
}

class ProducerSendCallback : public rocketmq::AutoDeleteSendCallback {
 public:
  struct CallbackData {
    std::unique_ptr<rocketmq::SendResult> result;
    std::exception_ptr exception;
    std::unique_ptr<Napi::ObjectReference> prevent_gc;
  };

 private:
  struct CleanupContext {
    std::unique_ptr<CallbackData> pending;
  };

 public:
  ProducerSendCallback(Napi::Env env, Napi::ObjectReference&& producer_ref, Napi::Function&& callback)
      : prevent_gc_(new Napi::ObjectReference(std::move(producer_ref))),
        cleanup_ctx_(nullptr),
        callback_(),
        prevent_prevent_release_(false) {
    std::unique_ptr<CleanupContext> ctx(new CleanupContext());
    callback_ = Callback::New(env,
                              callback,
                              "RocketMQ Send Callback",
                              0,
                              1,
                              ctx.get(),
                              &Finalize,
                              static_cast<void*>(nullptr));
    cleanup_ctx_ = ctx.release();
  }

  ~ProducerSendCallback() {
    if (!prevent_prevent_release_.exchange(true)) {
      callback_.Abort();
    }
  }

  void onSuccess(rocketmq::SendResult& send_result) override {
    ScheduleCallback(std::unique_ptr<rocketmq::SendResult>(
                         new rocketmq::SendResult(send_result)),
                     nullptr);
  }

  void onException(rocketmq::MQException& exception) noexcept override {
    ScheduleCallback(nullptr, std::make_exception_ptr(exception));
  }

 private:
  void ScheduleCallback(std::unique_ptr<rocketmq::SendResult> result,
                        std::exception_ptr exception) {
    auto prevent_gc = std::move(prevent_gc_);
    auto* data = new CallbackData{
        std::move(result),
        std::move(exception),
        std::move(prevent_gc)
    };

    napi_status status = napi_ok;
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
    if (IsEnvEnabled("ROCKETMQ_STUB_PRODUCER_BLOCKING_FAIL")) {
      status = napi_generic_failure;
    } else {
      status = callback_.BlockingCall(data);
    }
#else
    status = callback_.BlockingCall(data);
#endif
    if (status != napi_ok) {
      fprintf(stderr, "Failed to schedule JavaScript callback: %d\n", status);
      cleanup_ctx_->pending.reset(data);
    }

    if (!prevent_prevent_release_.exchange(true)) {
      callback_.Release();
    }
  }

  static void CallJs(Napi::Env env, Napi::Function callback, CleanupContext*, CallbackData* data) {
    std::unique_ptr<CallbackData> guard(data);
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
    if (env == nullptr || callback == nullptr || IsEnvEnabled("ROCKETMQ_STUB_PRODUCER_CALLJS_NULL_ENV")) {
#else
    if (env == nullptr || callback == nullptr) {
#endif
      return;
    }

    Napi::HandleScope scope(env);
    try {
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
      if (IsEnvEnabled("ROCKETMQ_STUB_PRODUCER_CALLJS_THROW")) {
        throw Napi::Error::New(env, "producer calljs throw");
      }
#endif
      if (data->exception) {
        try {
          std::rethrow_exception(data->exception);
        } catch (const std::exception& e) {
          callback.Call(env.Global(),
                        {Napi::Error::New(env, e.what()).Value()});
        }
      } else {
        callback.Call(env.Global(),
                      {env.Undefined(),
                       Napi::Number::New(env, data->result->send_status()),
                       Napi::String::New(env, data->result->msg_id()),
                       Napi::Number::New(env, data->result->queue_offset())});
      }
    } catch (const Napi::Error& e) {
      e.ThrowAsJavaScriptException();
    }
  }

  static void Finalize(Napi::Env, void*, CleanupContext* ctx) {
    delete ctx;
  }

  using Callback = Napi::TypedThreadSafeFunction<CleanupContext,
                                                 CallbackData,
                                                 &CallJs>;

  std::unique_ptr<Napi::ObjectReference> prevent_gc_;
  CleanupContext* cleanup_ctx_;
  Callback callback_;
  std::atomic<bool> prevent_prevent_release_;
};

Napi::Value RocketMQProducer::Send(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if required parameters are provided
  if (info.Length() < 4) {
    Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (!info[0].IsString()) {
    Napi::TypeError::New(env, "Topic must be a string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (!info[3].IsFunction()) {
    Napi::TypeError::New(env, "Callback must be a function").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  rocketmq::MQMessage message = [&]() {
    Napi::String topic = info[0].As<Napi::String>();
    Napi::Value body = info[1];
    if (body.IsString()) {
      return rocketmq::MQMessage(topic, body.ToString());
    } else if (body.IsBuffer()) {
      Napi::Buffer<char> buffer = body.As<Napi::Buffer<char>>();
      return rocketmq::MQMessage(topic,
                                 std::string(buffer.Data(), buffer.Length()));
    } else {
      Napi::TypeError::New(env, "Message body must be a string or buffer").ThrowAsJavaScriptException();
      return rocketmq::MQMessage("", "");
    }
  }();

  // If there was an error creating the message, return early
  if (message.topic().empty() && message.body().empty()) {
    return env.Undefined();
  }

  const Napi::Value options_v = info[2];
  if (options_v.IsObject()) {
    const Napi::Object options = options_v.ToObject();

    Napi::Value tags = options.Get("tags");
    if (tags.IsString()) {
      message.set_tags(tags.ToString());
    }

    Napi::Value keys = options.Get("keys");
    if (keys.IsString()) {
      message.set_keys(keys.ToString());
    }
  }

  auto* send_callback =
      new ProducerSendCallback(env, Napi::Persistent(Value()), info[3].As<Napi::Function>());
  try {
    producer_.send(message, send_callback);
  } catch (const std::exception& e) {
    delete send_callback;
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return env.Undefined();
}

}  // namespace __node_rocketmq__
