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

#include <exception>
#include <future>

#include <napi.h>

#include <ClientRPCHook.h>
#include <LoggerConfig.h>
#include <MQMessageListener.h>

#include "consumer_ack.h"

using namespace std;

namespace __node_rocketmq__ {

Napi::Object RocketMQPushConsumer::Init(Napi::Env env, Napi::Object exports) {
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

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData<Napi::FunctionReference>(constructor);

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
  consumer_.shutdown();
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
                      const rocketmq::DefaultMQPushConsumer& consumer)
      : Napi::AsyncWorker(callback), consumer_(consumer) {}

  void Execute() override { consumer_.start(); }

 private:
  rocketmq::DefaultMQPushConsumer consumer_;
};

Napi::Value RocketMQPushConsumer::Start(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if callback is provided and is a function
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Function expected as first argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  // AsyncWorker is automatically deleted after execution
  auto* worker = new ConsumerStartWorker(callback, consumer_);
  worker->Queue();
  return env.Undefined();
}

class ConsumerShutdownWorker : public Napi::AsyncWorker {
 public:
  ConsumerShutdownWorker(const Napi::Function& callback,
                         const rocketmq::DefaultMQPushConsumer& consumer)
      : Napi::AsyncWorker(callback), consumer_(consumer) {}

  void Execute() override { consumer_.shutdown(); }

 private:
  rocketmq::DefaultMQPushConsumer consumer_;
};

Napi::Value RocketMQPushConsumer::Shutdown(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  // Check if callback is provided and is a function
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "Function expected as first argument").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Function callback = info[0].As<Napi::Function>();

  // AsyncWorker is automatically deleted after execution
  auto* worker = new ConsumerShutdownWorker(callback, consumer_);
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
  // Ensure data is valid
  if (data == nullptr) {
    return;
  }

  // Handle case where env or listener is null
  if (env == nullptr || listener == nullptr) {
    try {
      data->promise.set_value(false);
    } catch (const std::future_error&) {
      // ignore
    }
    return;
  }

  try {
    Napi::Object message = Napi::Object::New(env);
    message.Set("topic", data->message.topic());
    message.Set("tags", data->message.tags());
    message.Set("keys", data->message.keys());
    message.Set("body", data->message.body());
    message.Set("msgId", data->message.msg_id());

    Napi::Object ack = ConsumerAck::NewInstance(env);
    if (ack.IsEmpty()) {
      // Failed to create ack object
      data->promise.set_value(false);
      return;
    }

    ConsumerAck* consumer_ack = Napi::ObjectWrap<ConsumerAck>::Unwrap(ack);
    if (consumer_ack == nullptr) {
      // Failed to unwrap ack object
      data->promise.set_value(false);
      return;
    }

    consumer_ack->SetPromise(std::move(data->promise));

    try {
      listener.Call(Napi::Object::New(listener.Env()), {message, ack});
    } catch (const std::exception&) {
      try {
        consumer_ack->Done(std::current_exception());
      } catch (const std::future_error&) {
        // ignore
      }
    }
  } catch (const std::exception&) {
    // If anything fails, set promise to false
    try {
      data->promise.set_value(false);
    } catch (const std::future_error&) {
      // ignore
    }
  }
}

class ConsumerMessageListener : public rocketmq::MessageListenerConcurrently {
 public:
  ConsumerMessageListener(Napi::Env& env, Napi::Function&& callback)
      : listener_(
            Listener::New(env, callback, "RocketMQ Message Listener", 0, 1)) {}

  ~ConsumerMessageListener() { listener_.Release(); }

  rocketmq::ConsumeStatus consumeMessage(
      std::vector<rocketmq::MQMessageExt>& msgs) override {
    for (auto& msg : msgs) {
      MessageAndPromise data{msg, std::promise<bool>()};
      auto future = data.promise.get_future();
      listener_.BlockingCall(&data);
      try {
        if (!future.get()) {
          return rocketmq::ConsumeStatus::RECONSUME_LATER;
        }
      } catch (const std::future_error& e) {
        // ignore
      } catch (const std::exception& e) {
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
};

Napi::Value RocketMQPushConsumer::SetListener(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  listener_.reset(
      new ConsumerMessageListener(env, info[0].As<Napi::Function>()));
  consumer_.registerMessageListener(listener_.get());
  return env.Undefined();
}

}  // namespace __node_rocketmq__
