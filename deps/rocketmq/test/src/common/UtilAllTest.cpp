#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

#include "ByteArray.h"
#include "common/UtilAll.h"

using rocketmq::ByteArray;
using rocketmq::UtilAll;

TEST(UtilAllTest, TrimAndIsBlank) {
  std::string padded = "  value  ";
  UtilAll::Trim(padded);
  EXPECT_EQ("value", padded);

  std::string tabs = "\tvalue\t";
  UtilAll::Trim(tabs);
  // Trim only strips spaces, so tabs remain.
  EXPECT_EQ("\tvalue\t", tabs);

  EXPECT_TRUE(UtilAll::isBlank(" \t\r\n"));
  EXPECT_FALSE(UtilAll::isBlank("  data"));
}

TEST(UtilAllTest, SplitWithCharDelimiter) {
  std::vector<std::string> parts;
  EXPECT_EQ(3, UtilAll::Split(parts, ",a,,b,c,", ','));
  ASSERT_EQ(3u, parts.size());
  EXPECT_EQ("a", parts[0]);
  EXPECT_EQ("b", parts[1]);
  EXPECT_EQ("c", parts[2]);
}

TEST(UtilAllTest, SplitWithStringDelimiter) {
  std::vector<std::string> parts;
  EXPECT_EQ(2, UtilAll::Split(parts, "||east||west||", "||"));
  ASSERT_EQ(2u, parts.size());
  EXPECT_EQ("east", parts[0]);
  EXPECT_EQ("west", parts[1]);
}

TEST(UtilAllTest, SplitURLParsesAddressAndPort) {
  std::string addr;
  short port = 0;
  ASSERT_TRUE(UtilAll::SplitURL("localhost:9876", addr, port));
  EXPECT_EQ("127.0.0.1", addr);
  EXPECT_EQ(9876, port);

  EXPECT_FALSE(UtilAll::SplitURL("noColon", addr, port));
  EXPECT_FALSE(UtilAll::SplitURL("host:0", addr, port));
}

TEST(UtilAllTest, BytesToHexRoundTrip) {
  const std::array<char, 3> raw{{0x00, static_cast<char>(0xAB), 0x7F}};
  std::string hex = UtilAll::bytes2string(raw.data(), raw.size());
  EXPECT_EQ("00AB7F", hex);

  std::array<char, 3> decoded{{0}};
  UtilAll::string2bytes(decoded.data(), hex);
  EXPECT_EQ(raw[0], decoded[0]);
  EXPECT_EQ(raw[1], decoded[1]);
  EXPECT_EQ(raw[2], decoded[2]);
}

TEST(UtilAllTest, HashCodeMatchesJavaLikeCalculation) {
  EXPECT_EQ(96354, UtilAll::hash_code("abc"));
  EXPECT_EQ(0, UtilAll::hash_code(""));
}

TEST(UtilAllTest, DeflateAndInflateRoundTrip) {
  const std::string payload = "compress me please";
  std::string compressed;
  ASSERT_TRUE(UtilAll::deflate(payload, compressed, 5));
  EXPECT_FALSE(compressed.empty());

  std::string restored;
  ASSERT_TRUE(UtilAll::inflate(compressed, restored));
  EXPECT_EQ(payload, restored);
}

TEST(UtilAllTest, StringToBoolIsCaseInsensitive) {
  EXPECT_TRUE(UtilAll::stob("true"));
  EXPECT_TRUE(UtilAll::stob("TRUE"));
  EXPECT_FALSE(UtilAll::stob("false"));
  EXPECT_FALSE(UtilAll::stob("yes"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "UtilAllTest.*";
  return RUN_ALL_TESTS();
}
