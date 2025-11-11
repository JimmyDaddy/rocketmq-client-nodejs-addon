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

#include <cstddef>
#include <exception>
#include <string>

#include <napi.h>

#include <ClientRPCHook.h>
#include <LoggerConfig.h>
#include <MQException.h>
#include <MQMessage.h>
#include <SendCallback.h>

namespace __node_rocketmq__ {

static void DeleteFunctionReference(Napi::Env, Napi::FunctionReference* data) {
  delete data;
}

Napi::Object RocketMQProducer::Init(Napi::Env env, Napi::Object exports) {
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

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData<Napi::FunctionReference, DeleteFunctionReference>(constructor);

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
                      rocketmq::DefaultMQProducer* producer)
      : Napi::AsyncWorker(callback), producer_(producer) {}

  void Execute() override { producer_->start(); }

 private:
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

  // AsyncWorker is automatically deleted after execution
  auto* worker = new ProducerStartWorker(callback, &producer_);
  worker->Queue();
  return env.Undefined();
}

class ProducerShutdownWorker : public Napi::AsyncWorker {
 public:
  ProducerShutdownWorker(const Napi::Function& callback,
                         rocketmq::DefaultMQProducer* producer)
      : Napi::AsyncWorker(callback), producer_(producer) {}

  void Execute() override { producer_->shutdown(); }

 private:
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

  // AsyncWorker is automatically deleted after execution
  auto* worker = new ProducerShutdownWorker(callback, &producer_);
  worker->Queue();
  return env.Undefined();
}

struct ResultOrException {
  std::unique_ptr<rocketmq::SendResult> result;
  std::exception_ptr exception;
};

void CallProducerSendJsCallback(Napi::Env env,
                                Napi::Function callback,
                                std::nullptr_t*,
                                ResultOrException* data) {
  std::unique_ptr<ResultOrException> data_guard(data);
  if (env != nullptr) {
    if (callback != nullptr) {
      if (data->exception) {
        try {
          std::rethrow_exception(data->exception);
        } catch (const std::exception& e) {
          callback.Call(Napi::Object::New(callback.Env()),
                        {Napi::Error::New(env, e.what()).Value()});
        }
      } else {
        callback.Call(Napi::Object::New(callback.Env()),
                      {env.Undefined(),
                       Napi::Number::New(env, data->result->send_status()),
                       Napi::String::New(env, data->result->msg_id()),
                       Napi::Number::New(env, data->result->queue_offset())});
      }
    }
  }
}

class ProducerSendCallback : public rocketmq::AutoDeleteSendCallback {
 public:
  ProducerSendCallback(Napi::Env&& env, Napi::Function&& callback)
      : callback_(
            Callback::New(env, callback, "RocketMQ Send Callback", 0, 1)),
        released_(false) {}

  ~ProducerSendCallback() {
    if (!released_) {
      callback_.Release();
      released_ = true;
    }
  }

  void onSuccess(rocketmq::SendResult& send_result) override {
    auto* data =
        new ResultOrException{std::unique_ptr<rocketmq::SendResult>(
                                  new rocketmq::SendResult(send_result)),
                              nullptr};

    napi_status status = callback_.BlockingCall(data);
    if (status != napi_ok) {
      delete data;
      fprintf(stderr, "Failed to call JavaScript callback in ProducerSendCallback::onSuccess\n");
    }

    if (!released_) {
      callback_.Release();
      released_ = true;
    }
  }

  void onException(rocketmq::MQException& exception) noexcept override {
    auto* data =
        new ResultOrException{nullptr, std::make_exception_ptr(exception)};

    napi_status status = callback_.BlockingCall(data);
    if (status != napi_ok) {
      delete data;
      fprintf(stderr, "Failed to call JavaScript callback in ProducerSendCallback::onException: %s\n",
              exception.what());
    }

    if (!released_) {
      callback_.Release();
      released_ = true;
    }
  }

 private:
  using Callback = Napi::TypedThreadSafeFunction<std::nullptr_t,
                                                 ResultOrException,
                                                 &CallProducerSendJsCallback>;

  Callback callback_;
  bool released_;
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

  // ProducerSendCallback is derived from AutoDeleteSendCallback, which is deleted by the RocketMQ client
  auto* send_callback =
      new ProducerSendCallback(info.Env(), info[3].As<Napi::Function>());
  producer_.send(message, send_callback);

  return env.Undefined();
}

}  // namespace __node_rocketmq__
