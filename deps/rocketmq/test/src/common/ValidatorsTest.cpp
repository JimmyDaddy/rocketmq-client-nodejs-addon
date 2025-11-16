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
#include <gtest/gtest.h>

#include <string>

#include "MQMessage.h"
#include "MQProtos.h"
#include "common/Validators.h"

#if defined(__GNUC__) && ((__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ <= 8))
#define ROCKETMQ_REGEX_SUPPORTED 0
#else
#define ROCKETMQ_REGEX_SUPPORTED 1
#endif

using rocketmq::MQClientException;
using rocketmq::MQMessage;
using rocketmq::Validators;

TEST(ValidatorsTest, RegularExpressionMatcher) {
  EXPECT_FALSE(Validators::regularExpressionMatcher("", "^.*$"));
  EXPECT_TRUE(Validators::regularExpressionMatcher("Group_01", "^[A-Za-z0-9_]+$"));
#if ROCKETMQ_REGEX_SUPPORTED
  EXPECT_FALSE(Validators::regularExpressionMatcher("bad space", "^[A-Za-z0-9_]+$"));
#endif
}

TEST(ValidatorsTest, GetGroupWithRegularExpression) {
#if ROCKETMQ_REGEX_SUPPORTED
  EXPECT_EQ("demo", Validators::getGroupWithRegularExpression("log_demo", "^log_(.*)$"));
#else
  EXPECT_EQ("", Validators::getGroupWithRegularExpression("log_demo", "^log_(.*)$"));
#endif
  EXPECT_EQ("", Validators::getGroupWithRegularExpression("log-demo", "^log_(.*)$"));
}

TEST(ValidatorsTest, CheckTopicValidations) {
  EXPECT_NO_THROW(Validators::checkTopic("Topic_ok-1"));
  EXPECT_THROW(Validators::checkTopic(""), MQClientException);
  EXPECT_THROW(Validators::checkTopic(rocketmq::AUTO_CREATE_TOPIC_KEY_TOPIC), MQClientException);
  EXPECT_THROW(Validators::checkTopic(std::string(256, 'a')), MQClientException);
#if ROCKETMQ_REGEX_SUPPORTED
  EXPECT_THROW(Validators::checkTopic("topic with space"), MQClientException);
#endif
}

TEST(ValidatorsTest, CheckGroupValidations) {
  EXPECT_NO_THROW(Validators::checkGroup("Group_ok-1"));
  EXPECT_THROW(Validators::checkGroup(""), MQClientException);
  EXPECT_THROW(Validators::checkGroup(std::string(256, 'b')), MQClientException);
#if ROCKETMQ_REGEX_SUPPORTED
  EXPECT_THROW(Validators::checkGroup("group*bad"), MQClientException);
#endif
}

TEST(ValidatorsTest, CheckMessageValidations) {
  MQMessage valid("Topic", "*", "body");
  EXPECT_NO_THROW(Validators::checkMessage(valid, 16));

  MQMessage empty("Topic", "*", "");
  EXPECT_THROW(Validators::checkMessage(empty, 16), MQClientException);

  MQMessage oversized("Topic", "*", std::string(8, 'c'));
  EXPECT_THROW(Validators::checkMessage(oversized, 4), MQClientException);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ValidatorsTest.*";
  return RUN_ALL_TESTS();
}
