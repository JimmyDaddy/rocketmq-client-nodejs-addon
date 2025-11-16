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
#include "c/CPullConsumer.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "CErrorContainer.h"
#include "ClientRPCHook.h"
#include "DefaultLitePullConsumer.h"
#include "Logging.h"
#include "MQMessageQueue.h"
#include "PullResult.h"
#include "SessionCredentials.h"

using rocketmq::CErrorContainer;
using rocketmq::ClientRPCHook;
using rocketmq::DefaultLitePullConsumer;
using rocketmq::MQMessageQueue;
using rocketmq::PullResult;
using rocketmq::PullStatus;
using rocketmq::SessionCredentials;

namespace {

bool isNullOrEmpty(const char* value) { return value == nullptr || *value == '\0'; }

CPullStatus ToCPullStatus(PullStatus status) {
  switch (status) {
    case PullStatus::FOUND:
      return E_FOUND;
    case PullStatus::NO_MATCHED_MSG:
      return E_NO_MATCHED_MSG;
    case PullStatus::OFFSET_ILLEGAL:
      return E_OFFSET_ILLEGAL;
    case PullStatus::NO_NEW_MSG:
    case PullStatus::NO_LATEST_MSG:
    default:
      return E_NO_NEW_MSG;
  }
}

struct CPullConsumerWrapper {
  std::unique_ptr<DefaultLitePullConsumer> consumer;
  std::shared_ptr<ClientRPCHook> rpc_hook;
  std::string namesrv_domain;
};

DefaultLitePullConsumer* unwrap(CPullConsumer* consumer) {
  auto* wrapper = reinterpret_cast<CPullConsumerWrapper*>(consumer);
  return wrapper == nullptr ? nullptr : wrapper->consumer.get();
}

}  // namespace

CPullConsumer* CreatePullConsumer(const char* groupId) {
  if (isNullOrEmpty(groupId)) {
    return nullptr;
  }

  auto* wrapper = new (std::nothrow) CPullConsumerWrapper();
  if (wrapper == nullptr) {
    return nullptr;
  }

  try {
    wrapper->consumer.reset(new DefaultLitePullConsumer(groupId));
  } catch (std::exception& e) {
    CErrorContainer::setErrorMessage(e.what());
    delete wrapper;
    return nullptr;
  }

  return reinterpret_cast<CPullConsumer*>(wrapper);
}

int DestroyPullConsumer(CPullConsumer* consumer) {
  if (consumer == nullptr) {
    return NULL_POINTER;
  }
  auto* wrapper = reinterpret_cast<CPullConsumerWrapper*>(consumer);
  delete wrapper;
  return OK;
}

int StartPullConsumer(CPullConsumer* consumer) {
  auto* lite_consumer = unwrap(consumer);
  if (lite_consumer == nullptr) {
    return NULL_POINTER;
  }
  try {
    lite_consumer->start();
  } catch (std::exception& e) {
    CErrorContainer::setErrorMessage(e.what());
    return PULLCONSUMER_START_FAILED;
  }
  return OK;
}

int ShutdownPullConsumer(CPullConsumer* consumer) {
  auto* lite_consumer = unwrap(consumer);
  if (lite_consumer == nullptr) {
    return NULL_POINTER;
  }
  try {
    lite_consumer->shutdown();
  } catch (std::exception& e) {
    CErrorContainer::setErrorMessage(e.what());
    return PULLCONSUMER_FETCH_MESSAGE_FAILED;
  }
  return OK;
}

int SetPullConsumerGroupID(CPullConsumer* consumer, const char* groupId) {
  auto* lite_consumer = unwrap(consumer);
  if (lite_consumer == nullptr || isNullOrEmpty(groupId)) {
    return NULL_POINTER;
  }
  lite_consumer->set_group_name(groupId);
  return OK;
}

const char* GetPullConsumerGroupID(CPullConsumer* consumer) {
  auto* lite_consumer = unwrap(consumer);
  if (lite_consumer == nullptr) {
    return nullptr;
  }
  return lite_consumer->group_name().c_str();
}

int SetPullConsumerNameServerAddress(CPullConsumer* consumer, const char* namesrv) {
  auto* lite_consumer = unwrap(consumer);
  if (lite_consumer == nullptr || isNullOrEmpty(namesrv)) {
    return NULL_POINTER;
  }
  lite_consumer->set_namesrv_addr(namesrv);
  return OK;
}

int SetPullConsumerNameServerDomain(CPullConsumer* consumer, const char* domain) {
  auto* wrapper = reinterpret_cast<CPullConsumerWrapper*>(consumer);
  if (wrapper == nullptr || isNullOrEmpty(domain)) {
    return NULL_POINTER;
  }
  wrapper->namesrv_domain = domain;
  return NOT_SUPPORT_NOW;
}

int SetPullConsumerSessionCredentials(CPullConsumer* consumer,
                                      const char* accessKey,
                                      const char* secretKey,
                                      const char* channel) {
  auto* wrapper = reinterpret_cast<CPullConsumerWrapper*>(consumer);
  if (wrapper == nullptr || isNullOrEmpty(accessKey) || isNullOrEmpty(secretKey) || isNullOrEmpty(channel)) {
    return NULL_POINTER;
  }

  SessionCredentials credentials(accessKey, secretKey, channel);
  wrapper->rpc_hook = std::make_shared<ClientRPCHook>(credentials);
  wrapper->consumer->setRPCHook(wrapper->rpc_hook);
  return OK;
}

int SetPullConsumerLogPath(CPullConsumer* consumer, const char* logPath) {
  if (consumer == nullptr || isNullOrEmpty(logPath)) {
    return NULL_POINTER;
  }
  // Logging path configuration is handled globally in current client, so just accept the value.
  return OK;
}

int SetPullConsumerLogFileNumAndSize(CPullConsumer* consumer, int /*fileNum*/, long /*fileSize*/) {
  if (consumer == nullptr) {
    return NULL_POINTER;
  }
  return OK;
}

int SetPullConsumerLogLevel(CPullConsumer* consumer, CLogLevel /*level*/) {
  if (consumer == nullptr) {
    return NULL_POINTER;
  }
  return OK;
}

int FetchSubscriptionMessageQueues(CPullConsumer* consumer, const char* topic, CMessageQueue** mqs, int* size) {
  auto* lite_consumer = unwrap(consumer);
  if (lite_consumer == nullptr || isNullOrEmpty(topic) || mqs == nullptr || size == nullptr) {
    return NULL_POINTER;
  }

  try {
    std::vector<MQMessageQueue> queues = lite_consumer->fetchMessageQueues(topic);
    *size = static_cast<int>(queues.size());
    if (*size == 0) {
      *mqs = nullptr;
      return OK;
    }

    auto* buffer = static_cast<CMessageQueue*>(std::malloc(sizeof(CMessageQueue) * queues.size()));
    if (buffer == nullptr) {
      *size = 0;
      *mqs = nullptr;
      return MALLOC_FAILED;
    }
    for (size_t i = 0; i < queues.size(); ++i) {
      auto& q = queues[i];
      std::strncpy(buffer[i].topic, q.topic().c_str(), MAX_TOPIC_LENGTH - 1);
      buffer[i].topic[MAX_TOPIC_LENGTH - 1] = '\0';
      std::strncpy(buffer[i].brokerName, q.broker_name().c_str(), MAX_BROKER_NAME_ID_LENGTH - 1);
      buffer[i].brokerName[MAX_BROKER_NAME_ID_LENGTH - 1] = '\0';
      buffer[i].queueId = q.queue_id();
    }
    *mqs = buffer;
  } catch (std::exception& e) {
    CErrorContainer::setErrorMessage(e.what());
    *mqs = nullptr;
    *size = 0;
    return PULLCONSUMER_FETCH_MQ_FAILED;
  }

  return OK;
}

int ReleaseSubscriptionMessageQueue(CMessageQueue* mqs) {
  if (mqs == nullptr) {
    return NULL_POINTER;
  }
  std::free(mqs);
  return OK;
}

CPullResult Pull(CPullConsumer* consumer,
                 const CMessageQueue* mq,
                 const char* subExpression,
                 long long offset,
                 int maxNums) {
  CPullResult response;
  std::memset(&response, 0, sizeof(CPullResult));
  response.pullStatus = E_NO_NEW_MSG;

  auto* lite_consumer = unwrap(consumer);
  if (lite_consumer == nullptr || mq == nullptr || maxNums <= 0) {
    return response;
  }

  std::string expression = (subExpression == nullptr || std::strlen(subExpression) == 0) ? "*" : subExpression;
  MQMessageQueue message_queue(mq->topic, mq->brokerName, mq->queueId);

  try {
    auto pull_result = lite_consumer->pullOnce(message_queue, expression, offset, maxNums, false,
                                              lite_consumer->consumer_pull_timeout_millis());
    if (!pull_result) {
      return response;
    }

    response.nextBeginOffset = pull_result->next_begin_offset();
    response.minOffset = pull_result->min_offset();
    response.maxOffset = pull_result->max_offset();
    response.pullStatus = ToCPullStatus(pull_result->pull_status());

    if (pull_result->pull_status() == PullStatus::FOUND && !pull_result->msg_found_list().empty()) {
      auto* stored = new PullResult(*pull_result);
      response.size = static_cast<int>(stored->msg_found_list().size());
      response.pData = stored;
      response.msgFoundList = static_cast<CMessageExt**>(std::malloc(sizeof(CMessageExt*) * response.size));
      if (response.msgFoundList == nullptr) {
        delete stored;
        response.size = 0;
        response.pData = nullptr;
        return response;
      }
      for (int i = 0; i < response.size; ++i) {
        auto msg = stored->msg_found_list()[i];
        response.msgFoundList[i] = reinterpret_cast<CMessageExt*>(msg.get());
      }
    }
  } catch (std::exception& e) {
    CErrorContainer::setErrorMessage(e.what());
    response.pullStatus = E_NO_NEW_MSG;
  }

  return response;
}

int ReleasePullResult(CPullResult pullResult) {
  if (pullResult.msgFoundList != nullptr) {
    std::free(pullResult.msgFoundList);
  }
  if (pullResult.pData != nullptr) {
    auto* stored = reinterpret_cast<PullResult*>(pullResult.pData);
    delete stored;
  }
  return OK;
}
