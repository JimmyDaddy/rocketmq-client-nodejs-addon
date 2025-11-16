#include <gtest/gtest.h>

#include "c/CPullConsumer.h"

namespace {

TEST(CPullConsumerTest, CreateAndDestroyWithoutStart) {
  auto* consumer = CreatePullConsumer("TestGroupLifecycle");
  ASSERT_NE(nullptr, consumer);

  EXPECT_EQ(NULL_POINTER, SetPullConsumerGroupID(consumer, nullptr));
  EXPECT_EQ(NULL_POINTER, SetPullConsumerNameServerAddress(consumer, nullptr));
  EXPECT_EQ(NULL_POINTER, SetPullConsumerSessionCredentials(consumer, nullptr, "", ""));

  EXPECT_EQ(OK, DestroyPullConsumer(consumer));
}

TEST(CPullConsumerTest, RejectsInvalidGroupOnCreateOrSet) {
  EXPECT_EQ(nullptr, CreatePullConsumer(nullptr));
  EXPECT_EQ(nullptr, CreatePullConsumer(""));

  auto* consumer = CreatePullConsumer("InitialGroup");
  ASSERT_NE(nullptr, consumer);
  EXPECT_EQ(NULL_POINTER, SetPullConsumerGroupID(consumer, ""));
  EXPECT_STREQ("InitialGroup", GetPullConsumerGroupID(consumer));
  EXPECT_EQ(OK, DestroyPullConsumer(consumer));
}

TEST(CPullConsumerTest, FetchSubscriptionMessageQueuesValidatesInputs) {
  auto* consumer = CreatePullConsumer("FetchGuardGroup");
  ASSERT_NE(nullptr, consumer);

  CMessageQueue* queues = nullptr;
  int size = 0;
  EXPECT_EQ(NULL_POINTER, FetchSubscriptionMessageQueues(nullptr, "Topic", &queues, &size));
  EXPECT_EQ(NULL_POINTER, FetchSubscriptionMessageQueues(consumer, nullptr, &queues, &size));
  EXPECT_EQ(NULL_POINTER, FetchSubscriptionMessageQueues(consumer, "", &queues, &size));
  EXPECT_EQ(NULL_POINTER, FetchSubscriptionMessageQueues(consumer, "Topic", nullptr, &size));
  EXPECT_EQ(NULL_POINTER, FetchSubscriptionMessageQueues(consumer, "Topic", &queues, nullptr));

  EXPECT_EQ(OK, DestroyPullConsumer(consumer));
}

TEST(CPullConsumerTest, PullHandlesNullInputs) {
  CPullResult result = Pull(nullptr, nullptr, nullptr, 0, 0);
  EXPECT_EQ(E_NO_NEW_MSG, result.pullStatus);

  auto* consumer = CreatePullConsumer("PullNullGuard");
  ASSERT_NE(nullptr, consumer);
  result = Pull(consumer, nullptr, nullptr, 0, 0);
  EXPECT_EQ(E_NO_NEW_MSG, result.pullStatus);
  EXPECT_EQ(OK, DestroyPullConsumer(consumer));
}

}  // namespace
