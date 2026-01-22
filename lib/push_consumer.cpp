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
#include "push_consumer.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <future>
#include <stdexcept>

#include <napi.h>

#include <ClientRPCHook.h>
#include <LoggerConfig.h>
#include <MQMessageListener.h>

#include "addon_data.h"
#include "consumer_ack.h"

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

Napi::Object RocketMQPushConsumer::Init(Napi::Env env, Napi::Object exports, AddonData* addon_data) {
  Napi::Function func = DefineClass(
      env,
      "RocketMQPushConsumer",
      {
          InstanceMethod<&RocketMQPushConsumer::Start>("start"),
          InstanceMethod<&RocketMQPushConsumer::Shutdown>("shutdown"),
          InstanceMethod<&RocketMQPushConsumer::Subscribe>("subscribe"),
          InstanceMethod<&RocketMQPushConsumer::SetListener>("setListener"),
          InstanceMethod<&RocketMQPushConsumer::SetSessionCredentials>(
              "setSessionCredentials"),
      });

  addon_data->push_consumer_constructor = Napi::Persistent(func);

  exports.Set("PushConsumer", func);
  return exports;
}

RocketMQPushConsumer::RocketMQPushConsumer(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<RocketMQPushConsumer>(info), consumer_("") {
  const Napi::Value group_name = info[0];
  if (group_name.IsString()) {
    consumer_.set_group_name(group_name.ToString());
  }

  const Napi::Value instance_name = info[1];
  if (instance_name.IsString()) {
    consumer_.set_instance_name(instance_name.ToString());
  }

  const Napi::Value options = info[2];
  if (options.IsObject()) {
    // try to set options
    SetOptions(options.ToObject());
  }
}

RocketMQPushConsumer::~RocketMQPushConsumer() {
  try {
    consumer_.shutdown();
  } catch (const std::exception&) {
  }
  listener_.reset();
}

void RocketMQPushConsumer::SetOptions(const Napi::Object& options) {
  // set name server
  Napi::Value name_server = options.Get("nameServer");
  if (name_server.IsString()) {
    consumer_.set_namesrv_addr(name_server.ToString());
  }

  // set group name
  Napi::Value group_name = options.Get("groupName");
  if (group_name.IsString()) {
    consumer_.set_group_name(group_name.ToString());
  }

  // set thread count
  Napi::Value thread_count = options.Get("threadCount");
  if (thread_count.IsNumber()) {
    consumer_.set_consume_thread_nums(thread_count.ToNumber());
  }

  // set message batch max size
  Napi::Value max_batch_size = options.Get("maxBatchSize");
  if (max_batch_size.IsNumber()) {
    consumer_.set_consume_message_batch_max_size(max_batch_size.ToNumber());
  }

  Napi::Value max_reconsume_times = options.Get("maxReconsumeTimes");
  if (max_reconsume_times.IsNumber()) {
    consumer_.set_max_reconsume_times(max_reconsume_times.ToNumber());
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

Napi::Value RocketMQPushConsumer::SetSessionCredentials(
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
  consumer_.setRPCHook(rpc_hook);

  return env.Undefined();
}

class ConsumerStartWorker : public Napi::AsyncWorker {
 public:
  ConsumerStartWorker(const Napi::Function& callback,
                      RocketMQPushConsumer* wrapper)
      : Napi::AsyncWorker(callback),
        wrapper_ref_(Napi::Persistent(wrapper->Value())),
        consumer_(&wrapper->consumer_) {}

  void Execute() override {
    try {
      consumer_->start();
    } catch (const std::exception& e) {
      SetError(e.what());
    }
  }

 private:
  Napi::ObjectReference wrapper_ref_;
  rocketmq::DefaultMQPushConsumer* consumer_;
};

Napi::Value RocketMQPushConsumer::Start(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if callback is provided and is a function
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Function expected as first argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  auto* worker = new ConsumerStartWorker(callback, this);
  worker->Queue();
  return env.Undefined();
}

class ConsumerShutdownWorker : public Napi::AsyncWorker {
 public:
  ConsumerShutdownWorker(const Napi::Function& callback,
                         RocketMQPushConsumer* wrapper)
      : Napi::AsyncWorker(callback),
        wrapper_ref_(Napi::Persistent(wrapper->Value())),
        consumer_(&wrapper->consumer_) {}

  void Execute() override {
    try {
      consumer_->shutdown();
    } catch (const std::exception& e) {
      SetError(e.what());
    }
  }

 private:
  Napi::ObjectReference wrapper_ref_;
  rocketmq::DefaultMQPushConsumer* consumer_;
};

Napi::Value RocketMQPushConsumer::Shutdown(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if callback is provided and is a function
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Function expected as first argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  auto* worker = new ConsumerShutdownWorker(callback, this);
  worker->Queue();
  return env.Undefined();
}

Napi::Value RocketMQPushConsumer::Subscribe(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if required parameters are provided
  if (info.Length() < 2) {
    Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (!info[0].IsString() || !info[1].IsString()) {
    Napi::TypeError::New(env, "Topic and expression must be strings").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::String topic = info[0].As<Napi::String>();
  Napi::String expression = info[1].As<Napi::String>();

  try {
    consumer_.subscribe(topic, expression);
  } catch (const std::exception& e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return env.Undefined();
}

struct MessageAndPromise {
  rocketmq::MQMessageExt message;
  std::promise<bool> promise;
};

void CallConsumerMessageJsListener(Napi::Env env,
                                   Napi::Function listener,
                                   std::nullptr_t*,
                                   MessageAndPromise* data) {
  std::unique_ptr<MessageAndPromise> data_guard(data);
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
  if (data == nullptr || IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_NULL_DATA")) {
#else
  if (data == nullptr) {
#endif
    if (data != nullptr) {
      try {
        data->promise.set_value(false);
      } catch (const std::future_error&) {
      }
    }
    return;
  }

#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
  if (env == nullptr || listener == nullptr || IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_NULL_ENV")) {
#else
  if (env == nullptr || listener == nullptr) {
#endif
    try {
      data->promise.set_value(false);
    } catch (const std::future_error&) {
    }
    return;
  }

  Napi::HandleScope scope(env);
  try {
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
    if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_THROW")) {
      throw std::runtime_error("consumer stub throw");
    }
#endif

    Napi::Object message = Napi::Object::New(env);
    message.Set("topic", data->message.topic());
    message.Set("tags", data->message.tags());
    message.Set("keys", data->message.keys());
    message.Set("body", data->message.body());
    message.Set("msgId", data->message.msg_id());

#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
    Napi::Object ack = IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_ACK_EMPTY") ? Napi::Object()
                                                                        : ConsumerAck::NewInstance(env);
#else
    Napi::Object ack = ConsumerAck::NewInstance(env);
#endif
    if (ack.IsEmpty()) {
      data->promise.set_value(false);
      return;
    }

#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
    ConsumerAck* consumer_ack = IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_ACK_NULL")
                                    ? nullptr
                                    : Napi::ObjectWrap<ConsumerAck>::Unwrap(ack);
#else
    ConsumerAck* consumer_ack = Napi::ObjectWrap<ConsumerAck>::Unwrap(ack);
#endif
    if (consumer_ack == nullptr) {
      data->promise.set_value(false);
      return;
    }

#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
    if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_PROMISE_SET")) {
      try {
        data->promise.set_value(true);
      } catch (const std::future_error&) {
      }
    }
#endif

    consumer_ack->SetPromise(std::move(data->promise));

    try {
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
      if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_LISTENER_ERROR")) {
        throw Napi::Error::New(env, "consumer listener error");
      }
#endif
      listener.Call(Napi::Object::New(env), {message, ack});
    } catch (const Napi::Error& e) {
      consumer_ack->Done(std::current_exception());
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
      if (!IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_LISTENER_ERROR")) {
        e.ThrowAsJavaScriptException();
      }
#else
      e.ThrowAsJavaScriptException();
#endif
    }
  } catch (const std::exception&) {
    try {
      data->promise.set_value(false);
    } catch (const std::future_error&) {
    }
  }
}

class ConsumerMessageListener : public rocketmq::MessageListenerConcurrently {
 public:
  ConsumerMessageListener(Napi::Env& env, Napi::Function&& callback)
      : listener_(
            Listener::New(env, callback, "RocketMQ Message Listener", 0, 1)),
        aborted_(false) {}

  ~ConsumerMessageListener() {
    if (!aborted_) {
      try {
        listener_.Release();
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
        if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_RELEASE_THROW")) {
          throw std::runtime_error("consumer release throw");
        }
#endif
      } catch (...) {
      }
    }
  }

  rocketmq::ConsumeStatus consumeMessage(
      std::vector<rocketmq::MQMessageExt>& msgs) override {
    for (auto& msg : msgs) {
      auto* data = new MessageAndPromise{msg, std::promise<bool>()};
      auto future = data->promise.get_future();

#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
      if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET")) {
        bool preset = !IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_PROMISE_PRESET_FALSE");
        try {
          data->promise.set_value(preset);
          data->promise.set_value(preset);
        } catch (const std::future_error&) {
        }
      }

      if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_TIMEOUT_SKIP_CALL")) {
        std::unique_ptr<MessageAndPromise> data_guard(data);
        auto wait_time = IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_TIMEOUT")
                             ? std::chrono::milliseconds(0)
                             : std::chrono::seconds(30);
        if (future.wait_for(wait_time) == std::future_status::timeout) {
          return rocketmq::ConsumeStatus::RECONSUME_LATER;
        }
        if (!future.get()) {
          return rocketmq::ConsumeStatus::RECONSUME_LATER;
        }
        continue;
      }
#endif

      napi_status status = napi_ok;
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
      if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_ABORT_TSFN")) {
        listener_.Abort();
        aborted_ = true;
        status = napi_generic_failure;
      } else if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_BLOCKING_FAIL")) {
        status = napi_generic_failure;
      } else {
        status = listener_.BlockingCall(data);
      }
#else
      status = listener_.BlockingCall(data);
#endif
      if (status != napi_ok) {
        delete data;
        return rocketmq::ConsumeStatus::RECONSUME_LATER;
      }

      try {
#if defined(ROCKETMQ_COVERAGE) || defined(ROCKETMQ_USE_STUB)
        if (IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_FORCE_FUTURE_ERROR")) {
          std::promise<bool> probe;
          auto probe_future = probe.get_future();
          (void)probe_future;
          probe.get_future();
        }

        auto wait_time = IsEnvEnabled("ROCKETMQ_STUB_CONSUMER_TIMEOUT")
                             ? std::chrono::milliseconds(0)
                             : std::chrono::seconds(30);
#else
        auto wait_time = std::chrono::seconds(30);
#endif
        if (future.wait_for(wait_time) == std::future_status::timeout) {
          return rocketmq::ConsumeStatus::RECONSUME_LATER;
        }
        if (!future.get()) {
          return rocketmq::ConsumeStatus::RECONSUME_LATER;
        }
      } catch (const std::future_error&) {
        return rocketmq::ConsumeStatus::RECONSUME_LATER;
      } catch (const std::exception&) {
        return rocketmq::ConsumeStatus::RECONSUME_LATER;
      }
    }
    return rocketmq::ConsumeStatus::CONSUME_SUCCESS;
  };

 private:
  using Listener =
      Napi::TypedThreadSafeFunction<std::nullptr_t,
                                    MessageAndPromise,
                                    &CallConsumerMessageJsListener>;

  Listener listener_;
  std::atomic<bool> aborted_;
};

Napi::Value RocketMQPushConsumer::SetListener(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Function expected as first argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  listener_.reset(
      new ConsumerMessageListener(env, info[0].As<Napi::Function>()));
  consumer_.registerMessageListener(listener_.get());
  return env.Undefined();
}

}  // namespace __node_rocketmq__
