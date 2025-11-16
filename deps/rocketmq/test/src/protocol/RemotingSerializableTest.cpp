#include <gtest/gtest.h>

#include <cstring>
#include <sstream>

#include "ByteArray.h"
#include "MQException.h"
#include "protocol/RemotingSerializable.h"

using rocketmq::ByteArray;
using rocketmq::RemotingSerializable;

TEST(RemotingSerializableTest, PrettyAndPlainWritersDiffer) {
  Json::Value root;
  root["int"] = 42;
  root["string"] = "value";

  const std::string pretty = RemotingSerializable::toJson(root, true);
  const std::string plain = RemotingSerializable::toJson(root, false);

  ASSERT_NE(pretty, plain);
  EXPECT_NE(std::string::npos, pretty.find('\n'));
  EXPECT_EQ(std::string::npos, plain.find('\n'));

  Json::Value reparsed = RemotingSerializable::fromJson(plain);
  EXPECT_EQ(42, reparsed["int"].asInt());
  EXPECT_EQ("value", reparsed["string"].asString());
}

TEST(RemotingSerializableTest, ParsesFromByteArray) {
  const std::string json = R"({"flag":true,"name":"rocket"})";
  ByteArray bytes(json.size());
  std::memcpy(bytes.array(), json.data(), json.size());

  Json::Value result = RemotingSerializable::fromJson(bytes);
  EXPECT_TRUE(result["flag"].asBool());
  EXPECT_EQ("rocket", result["name"].asString());
}

TEST(RemotingSerializableTest, ThrowsOnInvalidJson) {
  EXPECT_THROW(RemotingSerializable::fromJson("not valid json"), rocketmq::MQException);

  std::istringstream stream("[");
  EXPECT_THROW(RemotingSerializable::fromJson(stream), rocketmq::MQException);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "RemotingSerializableTest.*";
  return RUN_ALL_TESTS();
}
