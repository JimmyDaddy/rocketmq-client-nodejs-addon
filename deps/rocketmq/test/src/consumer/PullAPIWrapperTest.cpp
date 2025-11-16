#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ByteArray.h"
#include "ByteBuffer.hpp"
#include "MQMessageConst.h"
#include "MQMessageQueue.h"
#include "MessageDecoder.h"
#include "PullResult.h"
#include "consumer/PullAPIWrapper.h"
#include "consumer/PullResultExt.hpp"
#include "protocol/heartbeat/SubscriptionData.hpp"

using rocketmq::ByteArray;
using rocketmq::ByteArrayRef;
using rocketmq::ByteBuffer;
using rocketmq::MQMessageConst;
using rocketmq::MQMessageQueue;
using rocketmq::PullAPIWrapper;
using rocketmq::PullResult;
using rocketmq::PullResultExt;
using rocketmq::SubscriptionData;

namespace {

struct EncodedMessageSpec {
  std::string topic;
  std::string tags;
  bool transaction_prepared;
  std::string uniq_id;
};

void AppendMessage(ByteBuffer& buffer, const EncodedMessageSpec& spec, int queue_id) {
  int32_t start = buffer.position();
  buffer.putInt(0);             // TOTALSIZE placeholder
  buffer.putInt(0);             // MAGICCODE
  buffer.putInt(0);             // BODYCRC
  buffer.putInt(queue_id);      // QUEUEID
  buffer.putInt(0);             // FLAG
  buffer.putLong(start);        // QUEUEOFFSET
  buffer.putLong(start * 10L);  // PHYSICALOFFSET
  buffer.putInt(0);             // SYSFLAG (IPv4 hosts)
  buffer.putLong(0);            // BORNTIMESTAMP
  buffer.putInt(ntohl(inet_addr("127.0.0.1")));
  buffer.putInt(10091);
  buffer.putLong(0);  // STORETIMESTAMP
  buffer.putInt(ntohl(inet_addr("127.0.0.2")));
  buffer.putInt(10092);
  buffer.putInt(0);   // RECONSUMETIMES
  buffer.putLong(0);  // PREPARED TRANSACTION OFFSET

  const std::string body = "body-" + spec.tags;
  buffer.putInt(body.size());
  buffer.put(ByteArray(const_cast<char*>(body.data()), body.size()));

  buffer.put(static_cast<int8_t>(spec.topic.size()));
  buffer.put(ByteArray(const_cast<char*>(spec.topic.data()), spec.topic.size()));

  std::map<std::string, std::string> properties;
  properties[MQMessageConst::PROPERTY_TAGS] = spec.tags;
  properties[MQMessageConst::PROPERTY_UNIQ_CLIENT_MESSAGE_ID_KEYIDX] = spec.uniq_id;
  properties[MQMessageConst::PROPERTY_TRANSACTION_PREPARED] = spec.transaction_prepared ? "true" : "false";
  std::string encoded_properties = rocketmq::MessageDecoder::messageProperties2String(properties);
  buffer.putShort(static_cast<int16_t>(encoded_properties.size()));
  buffer.put(ByteArray(const_cast<char*>(encoded_properties.data()), encoded_properties.size()));

  int32_t end = buffer.position();
  buffer.putInt(start, end - start);
}

ByteArrayRef BuildMessageBinary(const std::vector<EncodedMessageSpec>& specs) {
  auto buffer = std::unique_ptr<ByteBuffer>(ByteBuffer::allocate(4096));
  int queue_id = 0;
  for (const auto& spec : specs) {
    AppendMessage(*buffer, spec, queue_id++);
  }
  int total = buffer->position();
  std::string data(buffer->array(), total);
  return rocketmq::stoba(std::move(data));
}

}  // namespace

TEST(PullAPIWrapperTest, ProcessPullResultFiltersMessagesAndDecoratesMetadata) {
  PullAPIWrapper wrapper(nullptr, "GID_unit_test");
  MQMessageQueue mq("TopicFilter", "broker-a", 1);

  SubscriptionData subscription(mq.topic(), "TagA || TagB");
  subscription.tags_set().push_back("TagA");

  auto binary = BuildMessageBinary({
      {mq.topic(), "TagA", true, "TX-1"},
      {mq.topic(), "TagB", false, "TX-2"},
  });

  std::unique_ptr<PullResult> pull_result(new PullResultExt(rocketmq::FOUND, 11, 5, 20, 2, binary));
  std::unique_ptr<PullResult> processed(
      wrapper.processPullResult(mq, std::move(pull_result), &subscription));

  ASSERT_NE(nullptr, processed);
  ASSERT_EQ(1U, processed->msg_found_list().size());

  const auto& message = processed->msg_found_list()[0];
  EXPECT_EQ("TagA", message->tags());
  EXPECT_EQ("TX-1", message->transaction_id());
  EXPECT_EQ("5", message->getProperty(MQMessageConst::PROPERTY_MIN_OFFSET));
  EXPECT_EQ("20", message->getProperty(MQMessageConst::PROPERTY_MAX_OFFSET));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "PullAPIWrapperTest.*";
  return RUN_ALL_TESTS();
}
