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
#include "SocketUtil.h"

#include <cstdlib>  // std::abort
#include <cstring>  // std::memcpy, std::memset

#include <iostream>
#include <limits>
#include <memory>    // std::unique_ptr
#include <stdexcept>  // std::invalid_argument, std::runtime_error
#include <string>

#ifndef WIN32
#include <arpa/inet.h>  // htons
#include <unistd.h>     // gethostname
#else
#include <Winsock2.h>
#endif

#include <event2/event.h>

#include "MQException.h"

namespace rocketmq {

// 安全的内存拷贝函数，包含边界检查
void SafeMemcpy(void* dest, size_t dest_size, const void* src, size_t src_size) {
  if (dest == nullptr || src == nullptr) {
    throw std::invalid_argument("null pointer in memory copy");
  }
  if (src_size > dest_size) {
    throw std::invalid_argument("source size exceeds destination buffer size");
  }
  std::memcpy(dest, src, src_size);
}

std::unique_ptr<sockaddr_storage> SockaddrToStorage(const sockaddr* src) {
  if (src == nullptr) {
    return nullptr;
  }
  std::unique_ptr<sockaddr_storage> ss(new sockaddr_storage());
  // 不需要零初始化，因为 SafeMemcpy 会覆盖整个结构体
  SafeMemcpy(ss.get(), sizeof(sockaddr_storage), src, SockaddrSize(src));
  return ss;
}

std::unique_ptr<sockaddr_storage> IPPortToSockaddr(const ByteArray& ip, uint16_t port) {
  // 输入验证
  if (ip.array() == nullptr) {
    throw std::invalid_argument("null IP array");
  }

  std::unique_ptr<sockaddr_storage> ss(new sockaddr_storage());
  // 使用 volatile 防止编译器优化掉 memset
  volatile unsigned char* ptr = reinterpret_cast<volatile unsigned char*>(ss.get());
  for (size_t i = 0; i < sizeof(sockaddr_storage); ++i) {
    ptr[i] = 0;
  }

  if (ip.size() == kIPv4AddrSize) {
    auto* sin = reinterpret_cast<sockaddr_in*>(ss.get());
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    SafeMemcpy(&sin->sin_addr, sizeof(sin->sin_addr), ip.array(), kIPv4AddrSize);
  } else if (ip.size() == kIPv6AddrSize) {
    auto* sin6 = reinterpret_cast<sockaddr_in6*>(ss.get());
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    SafeMemcpy(&sin6->sin6_addr, sizeof(sin6->sin6_addr), ip.array(), kIPv6AddrSize);
  } else {
    throw std::invalid_argument("invalid ip size: " + std::to_string(ip.size()) +
                               " (expected " + std::to_string(kIPv4AddrSize) +
                               " or " + std::to_string(kIPv6AddrSize) + ")");
  }
  return ss;
}

std::unique_ptr<sockaddr_storage> StringToSockaddr(const std::string& addr) {
  if (addr.empty()) {
    throw std::invalid_argument("invalid address: empty string");
  }

  std::string::size_type start_pos = addr[0] == '/' ? 1 : 0;
  auto colon_pos = addr.find_last_of(':');
  auto bracket_pos = addr.find_last_of(']');

  if (bracket_pos != std::string::npos) {
    // ipv6 address
    if (addr.at(start_pos) != '[') {
      throw std::invalid_argument("invalid IPv6 address: missing opening bracket");
    }
    if (colon_pos == std::string::npos) {
      throw std::invalid_argument("invalid IPv6 address: missing colon separator");
    }
    if (colon_pos < bracket_pos) {
      // have not port
      if (bracket_pos != addr.size() - 1) {
        throw std::invalid_argument("invalid IPv6 address: unexpected characters after closing bracket");
      }
      colon_pos = addr.size();
    } else if (colon_pos != bracket_pos + 1) {
      throw std::invalid_argument("invalid IPv6 address: colon not immediately after closing bracket");
    }
  } else if (colon_pos == std::string::npos) {
    // have not port
    colon_pos = addr.size();
  }

  decltype(bracket_pos) fix_bracket = bracket_pos == std::string::npos ? 0 : 1;
  std::string host = addr.substr(start_pos + fix_bracket, colon_pos - start_pos - fix_bracket * 2);

  if (host.empty()) {
    throw std::invalid_argument("invalid address: empty hostname");
  }

  auto sa = LookupNameServers(host);

  std::string port_str = colon_pos >= addr.size() ? "" : addr.substr(colon_pos + 1);
  uint16_t port_num = 0;

  if (!port_str.empty()) {
    try {
      uint32_t n = std::stoul(port_str);
      if (n > std::numeric_limits<uint16_t>::max()) {
        throw std::out_of_range("port is too large: " + std::to_string(n) +
                               " (max: " + std::to_string(std::numeric_limits<uint16_t>::max()) + ")");
      }
      if (n == 0) {
        throw std::invalid_argument("port cannot be zero");
      }
      port_num = htons(static_cast<uint16_t>(n));
    } catch (const std::exception& e) {
      throw std::invalid_argument("invalid port: " + port_str + " (" + e.what() + ")");
    }
  }

  if (sa->ss_family == AF_INET) {
    auto* sin = reinterpret_cast<sockaddr_in*>(sa.get());
    sin->sin_port = port_num;
  } else if (sa->ss_family == AF_INET6) {
    auto* sin6 = reinterpret_cast<sockaddr_in6*>(sa.get());
    sin6->sin6_port = port_num;
  } else {
    throw std::runtime_error("don't support non-inet address families: " +
                            std::to_string(sa->ss_family));
  }

  return sa;
}

/**
 * converts an address from network format to presentation format
 */
std::string SockaddrToString(const sockaddr* addr) {
  if (nullptr == addr) {
    return std::string();
  }

  char buf[128] = {0};
  std::string address;
  uint16_t port = 0;

  if (addr->sa_family == AF_INET) {
    const auto* sin = reinterpret_cast<const sockaddr_in*>(addr);
    if (nullptr == evutil_inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
      throw std::runtime_error("can not convert AF_INET address to text form");
    }
    address = buf;
    port = ntohs(sin->sin_port);
  } else if (addr->sa_family == AF_INET6) {
    const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(addr);
    if (nullptr == evutil_inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf))) {
      throw std::runtime_error("can not convert AF_INET6 address to text form");
    }
    address = buf;
    port = ntohs(sin6->sin6_port);
  } else {
    throw std::runtime_error("don't support non-inet address families: " +
                            std::to_string(addr->sa_family));
  }

  if (addr->sa_family == AF_INET6) {
    address = "[" + address + "]";
  }
  if (port != 0) {
    address += ":" + std::to_string(port);
  }

  return address;
}

std::unique_ptr<sockaddr_storage> LookupNameServers(const std::string& hostname) {
  if (hostname.empty()) {
    throw std::invalid_argument("invalid hostname: empty string");
  }

  // 验证主机名格式
  if (hostname.length() > 253) {  // RFC 1123
    throw std::invalid_argument("hostname too long (max 253 characters)");
  }

  evutil_addrinfo hints;
  evutil_addrinfo* answer = nullptr;

  /* Build the hints to tell getaddrinfo how to act. */
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;           /* v4 or v6 is fine. */
  hints.ai_socktype = SOCK_STREAM;       /* We want stream socket*/
  hints.ai_protocol = IPPROTO_TCP;       /* We want a TCP socket */
  hints.ai_flags = EVUTIL_AI_ADDRCONFIG; /* Only return addresses we can use. */

  // Look up the hostname.
  int err = evutil_getaddrinfo(hostname.c_str(), nullptr, &hints, &answer);
  if (err != 0) {
    std::string info = "Failed to resolve hostname(" + hostname + "): " + evutil_gai_strerror(err);
    THROW_MQEXCEPTION(UnknownHostException, info, -1);
  }

  std::unique_ptr<sockaddr_storage> ss(new sockaddr_storage());
  // 不需要零初始化，因为 SafeMemcpy 会覆盖整个结构体

  bool hit = false;
  for (struct evutil_addrinfo* ai = answer; ai != nullptr; ai = ai->ai_next) {
    auto* ai_addr = ai->ai_addr;
    if (ai_addr->sa_family != AF_INET && ai_addr->sa_family != AF_INET6) {
      continue;
    }
    SafeMemcpy(ss.get(), sizeof(sockaddr_storage), ai_addr, SockaddrSize(ai_addr));
    hit = true;
    break;
  }

  evutil_freeaddrinfo(answer);

  if (!hit) {
    throw std::runtime_error("hostname '" + hostname + "' resolved to non-inet address family");
  }

  return ss;
}

std::unique_ptr<sockaddr_storage> GetSelfIP() {
  try {
    return LookupNameServers(GetLocalHostname());
  } catch (const UnknownHostException& e) {
    // 回退到localhost
    return LookupNameServers("localhost");
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to get self IP: " + std::string(e.what()));
  }
}

const std::string& GetLocalHostname() {
  static std::string local_hostname = []() {
    char name[1024] = {0};
    if (::gethostname(name, sizeof(name)) != 0) {
      return std::string("unknown");
    }
    return std::string(name);
  }();
  return local_hostname;
}

const std::string& GetLocalAddress() {
  static std::string local_address = []() {
    try {
      auto self_ip = GetSelfIP();
      return SockaddrToString(GetSockaddrPtr(self_ip));
    } catch (const std::exception& e) {
      // 使用更温和的错误处理，而不是abort
      std::cerr << "Warning: Failed to get local address: " << e.what() << std::endl;
      return std::string("127.0.0.1");  // 安全回退
    }
  }();
  return local_address;
}

}  // namespace rocketmq
