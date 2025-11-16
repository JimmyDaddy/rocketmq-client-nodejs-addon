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
#include "MessageClientIDSetter.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifndef WIN32
#include <unistd.h>
#endif

#include "ByteBuffer.hpp"
#include "SocketUtil.h"
#include "UtilAll.h"

namespace rocketmq {

namespace {

constexpr uint32_t kFnvOffsetBasis = 2166136261u;
constexpr uint32_t kFnvPrime = 16777619u;

uint32_t FoldIPv6Address(const struct sockaddr_in6* addr) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(&addr->sin6_addr);
  uint32_t hash = kFnvOffsetBasis;
  for (size_t i = 0; i < kIPv6AddrSize; ++i) {
    hash ^= bytes[i];
    hash *= kFnvPrime;
  }
  return ByteOrderUtil::NorminalBigEndian(hash);
}

void WriteIpBytes(const sockaddr* addr, std::array<uint8_t, kIPv4AddrSize>& ip_bytes, bool& initialized) {
  if (addr == nullptr || initialized) {
    return;
  }

  if (addr->sa_family == AF_INET) {
    auto* sin = (struct sockaddr_in*)addr;
    std::memcpy(ip_bytes.data(), &sin->sin_addr, kIPv4AddrSize);
    initialized = true;
    return;
  }

  if (addr->sa_family == AF_INET6) {
    auto* sin6 = (struct sockaddr_in6*)addr;
    auto folded = FoldIPv6Address(sin6);
    std::memcpy(ip_bytes.data(), &folded, kIPv4AddrSize);
    initialized = true;
  }
}

}  // namespace

MessageClientIDSetter::MessageClientIDSetter() {
  std::srand((uint32_t)std::time(NULL));

  std::array<uint8_t, kIPv4AddrSize> ip_bytes{};
  bool ip_initialized = false;
  auto self_ip = GetSelfIP();
  WriteIpBytes(GetSockaddrPtr(self_ip), ip_bytes, ip_initialized);

  if (!ip_initialized) {
    auto fallback = ByteOrderUtil::NorminalBigEndian(static_cast<uint32_t>(UtilAll::currentTimeMillis()));
    std::memcpy(ip_bytes.data(), &fallback, sizeof(fallback));
  }

  std::unique_ptr<ByteBuffer> buffer;
  buffer.reset(ByteBuffer::allocate(kIPv4AddrSize + sizeof(int16_t) + sizeof(int32_t)));
  buffer->put(ByteArray(reinterpret_cast<char*>(ip_bytes.data()), kIPv4AddrSize));
  buffer->putShort(UtilAll::getProcessId());
  buffer->putInt(std::rand());

  fixed_string_ = UtilAll::bytes2string(buffer->array(), buffer->position());

  setStartTime(UtilAll::currentTimeMillis());

  counter_ = 0;
}

MessageClientIDSetter::~MessageClientIDSetter() = default;

void MessageClientIDSetter::setStartTime(uint64_t millis) {
  // std::time_t
  //   Although not defined, this is almost always an integral value holding the number of seconds
  //   (not counting leap seconds) since 00:00, Jan 1 1970 UTC, corresponding to POSIX time.
  std::time_t tmNow = millis / 1000;
  std::tm tmResult;
#ifdef WIN32
  if (localtime_s(&tmResult, &tmNow) != 0) {
    start_time_ = millis;
    next_start_time_ = millis;
    return;
  }
  std::tm* ptmNow = &tmResult;
#else
  std::tm* ptmNow = localtime_r(&tmNow, &tmResult);
  if (ptmNow == nullptr) {
    start_time_ = millis;
    next_start_time_ = millis;
    return;
  }
#endif

  std::tm curMonthBegin = {0};
  curMonthBegin.tm_year = ptmNow->tm_year;  // since 1900
  curMonthBegin.tm_mon = ptmNow->tm_mon;    // [0, 11]
  curMonthBegin.tm_mday = 1;                // [1, 31]
  curMonthBegin.tm_hour = 0;                // [0, 23]
  curMonthBegin.tm_min = 0;                 // [0, 59]
  curMonthBegin.tm_sec = 0;                 // [0, 60]

  std::tm nextMonthBegin = {0};
  if (ptmNow->tm_mon >= 11) {
    nextMonthBegin.tm_year = ptmNow->tm_year + 1;
    nextMonthBegin.tm_mon = 0;
  } else {
    nextMonthBegin.tm_year = ptmNow->tm_year;
    nextMonthBegin.tm_mon = ptmNow->tm_mon + 1;
  }
  nextMonthBegin.tm_mday = 1;
  nextMonthBegin.tm_hour = 0;
  nextMonthBegin.tm_min = 0;
  nextMonthBegin.tm_sec = 0;

  start_time_ = std::mktime(&curMonthBegin) * 1000;
  next_start_time_ = std::mktime(&nextMonthBegin) * 1000;
}

std::string MessageClientIDSetter::createUniqueID() {
  uint64_t current = UtilAll::currentTimeMillis();
  if (current >= next_start_time_) {
    setStartTime(current);
    current = UtilAll::currentTimeMillis();
  }

  uint32_t period = ByteOrderUtil::NorminalBigEndian(static_cast<uint32_t>(current - start_time_));
  uint16_t seqid = ByteOrderUtil::NorminalBigEndian(counter_++);

  return fixed_string_ + UtilAll::bytes2string(reinterpret_cast<char*>(&period), sizeof(period)) +
         UtilAll::bytes2string(reinterpret_cast<char*>(&seqid), sizeof(seqid));
}

}  // namespace rocketmq
