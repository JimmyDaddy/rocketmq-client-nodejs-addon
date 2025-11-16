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
#include "VirtualEnvUtil.h"

#include <stdio.h>
#include <stdlib.h>

#include "UtilAll.h"

static const char* VIRTUAL_APPGROUP_PREFIX = "%%PROJECT_%s%%";

namespace rocketmq {

std::string VirtualEnvUtil::buildWithProjectGroup(const std::string& origin, const std::string& projectGroup) {
  if (UtilAll::isBlank(projectGroup)) {
    return origin;
  }

  char prefix[1024];
  sprintf(prefix, VIRTUAL_APPGROUP_PREFIX, projectGroup.c_str());
  std::string suffix(prefix);

  if (origin.size() >= suffix.size() &&
      origin.compare(origin.size() - suffix.size(), suffix.size(), suffix) == 0) {
    return origin;
  }

  return origin + suffix;
}

std::string VirtualEnvUtil::clearProjectGroup(const std::string& origin, const std::string& projectGroup) {
  if (UtilAll::isBlank(projectGroup)) {
    return origin;
  }

  char prefix[1024];
  sprintf(prefix, VIRTUAL_APPGROUP_PREFIX, projectGroup.c_str());
  std::string suffix(prefix);

  if (origin.size() < suffix.size()) {
    return origin;
  }

  auto pos = origin.rfind(suffix);
  if (pos != std::string::npos && pos + suffix.size() == origin.size()) {
    return origin.substr(0, pos);
  }

  return origin;
}

}  // namespace rocketmq
