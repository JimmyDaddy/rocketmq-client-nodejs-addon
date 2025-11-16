#include <gtest/gtest.h>

#include "MessageClientIDSetter.h"
#include "MessageExtImpl.h"
#include "MessageSysFlag.h"
#include "transport/SocketUtil.h"

using rocketmq::MessageClientExtImpl;
using rocketmq::MessageClientIDSetter;
using rocketmq::MessageExtImpl;
using rocketmq::MessageSysFlag;
using rocketmq::SockaddrToString;
using rocketmq::StringToSockaddr;
using rocketmq::TopicFilterType;

namespace {

std::unique_ptr<sockaddr_storage> MakeAddr(const std::string& text) {
  return rocketmq::StringToSockaddr(text);
}

}  // namespace

TEST(MessageExtImplTest, ParsesTopicFilterTypeFromSysFlag) {
  EXPECT_EQ(TopicFilterType::SINGLE_TAG, MessageExtImpl::parseTopicFilterType(0));
  EXPECT_EQ(TopicFilterType::MULTI_TAG,
            MessageExtImpl::parseTopicFilterType(MessageSysFlag::MULTI_TAGS_FLAG));
  EXPECT_EQ(TopicFilterType::MULTI_TAG,
            MessageExtImpl::parseTopicFilterType(MessageSysFlag::MULTI_TAGS_FLAG | 0x10));
}

TEST(MessageExtImplTest, HostConversionAndToStringIncludesFields) {
  auto born = MakeAddr("127.0.0.1:1234");
  auto store = MakeAddr("[::1]:4321");

  MessageExtImpl message(/*queueId*/ 2,
                         /*bornTimestamp*/ 111,
                         reinterpret_cast<sockaddr*>(born.get()),
                         /*storeTimestamp*/ 222,
                         reinterpret_cast<sockaddr*>(store.get()),
                         "MSG123");

  message.set_store_size(1024);
  message.set_body_crc(42);
  message.set_queue_offset(55);
  message.set_commit_log_offset(99);
  message.set_sys_flag(7);
  message.set_reconsume_times(3);
  message.set_prepared_transaction_offset(77);

  EXPECT_EQ("127.0.0.1:1234", message.born_host_string());
  EXPECT_EQ("[::1]:4321", message.store_host_string());

  const auto text = message.toString();
  EXPECT_NE(std::string::npos, text.find("queueId=2"));
  EXPECT_NE(std::string::npos, text.find("msgId=MSG123"));
  EXPECT_NE(std::string::npos, text.find("bornHost=127.0.0.1:1234"));
  EXPECT_NE(std::string::npos, text.find("storeHost=[::1]:4321"));
  EXPECT_NE(std::string::npos, text.find("commitLogOffset=99"));
}

TEST(MessageClientExtImplTest, MsgIdPrefersUniqIdOverOffset) {
  MessageClientExtImpl ext;
  ext.set_offset_msg_id("OFFSET-ID");
  ASSERT_EQ("OFFSET-ID", ext.msg_id());

  MessageClientIDSetter::setUniqID(ext);
  const auto uniq = ext.msg_id();
  EXPECT_NE("OFFSET-ID", uniq);
  EXPECT_EQ(uniq, MessageClientIDSetter::getUniqID(ext));
  EXPECT_EQ("OFFSET-ID", ext.offset_msg_id());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "MessageExtImplTest.*:MessageClientExtImplTest.*";
  return RUN_ALL_TESTS();
}
