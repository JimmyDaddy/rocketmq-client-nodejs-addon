#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "MQMessage.h"
#include "c/CBatchMessage.h"
#include "c/CMessage.h"

using rocketmq::MQMessage;

namespace {

struct BatchDeleter {
  void operator()(CBatchMessage* batch) const {
    if (batch) {
      DestroyBatchMessage(batch);
    }
  }
};

struct MessageDeleter {
  void operator()(CMessage* msg) const {
    if (msg) {
      DestroyMessage(msg);
    }
  }
};

using BatchPtr = std::unique_ptr<CBatchMessage, BatchDeleter>;
using MessagePtr = std::unique_ptr<CMessage, MessageDeleter>;

MessagePtr MakeMessage(const char* topic, const char* body) {
  MessagePtr message(CreateMessage(topic));
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(OK, SetMessageBody(message.get(), body));
  return message;
}

}  // namespace

TEST(CBatchMessageTest, CreateAddDestroyCopiesMessages) {
  BatchPtr batch(CreateBatchMessage());
  ASSERT_NE(nullptr, batch);

  auto first = MakeMessage("TopicA", "BodyA");
  auto second = MakeMessage("TopicB", "BodyB");

  EXPECT_EQ(OK, AddMessage(batch.get(), first.get()));
  EXPECT_EQ(OK, AddMessage(batch.get(), second.get()));

  auto* messages = reinterpret_cast<std::vector<MQMessage>*>(batch.get());
  ASSERT_EQ(2u, messages->size());
  EXPECT_EQ("TopicA", messages->at(0).topic());
  EXPECT_EQ("BodyA", messages->at(0).body());
  EXPECT_EQ("TopicB", messages->at(1).topic());
  EXPECT_EQ("BodyB", messages->at(1).body());
}

TEST(CBatchMessageTest, GuardsAgainstNullPointers) {
  BatchPtr batch(CreateBatchMessage());
  ASSERT_NE(nullptr, batch);

  auto message = MakeMessage("Topic", "Body");

  EXPECT_EQ(NULL_POINTER, AddMessage(nullptr, message.get()));
  EXPECT_EQ(NULL_POINTER, AddMessage(batch.get(), nullptr));
  EXPECT_EQ(NULL_POINTER, DestroyBatchMessage(nullptr));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "CBatchMessageTest.*";
  return RUN_ALL_TESTS();
}
