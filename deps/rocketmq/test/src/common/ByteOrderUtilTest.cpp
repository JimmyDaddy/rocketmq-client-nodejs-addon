#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "ByteOrder.h"

using rocketmq::ByteOrderUtil;

TEST(ByteOrderUtilTest, SwapFunctionsReverseBytes) {
  EXPECT_EQ(0x3412, ByteOrderUtil::swap(static_cast<uint16_t>(0x1234)));
  EXPECT_EQ(0x78563412, ByteOrderUtil::swap(static_cast<uint32_t>(0x12345678)));
  EXPECT_EQ(0x8877665544332211ULL, ByteOrderUtil::swap(0x1122334455667788ULL));
}

TEST(ByteOrderUtilTest, WriteProducesExpectedEndianness) {
  std::array<char, 4> data{};

  ByteOrderUtil::Write(data.data(), static_cast<uint32_t>(0x01020304), true);
  EXPECT_EQ(0x01, static_cast<uint8_t>(data[0]));
  EXPECT_EQ(0x02, static_cast<uint8_t>(data[1]));
  EXPECT_EQ(0x03, static_cast<uint8_t>(data[2]));
  EXPECT_EQ(0x04, static_cast<uint8_t>(data[3]));
  EXPECT_EQ(0x01020304, ByteOrderUtil::Read<uint32_t>(data.data(), true));

  ByteOrderUtil::Write(data.data(), static_cast<uint32_t>(0x01020304), false);
  EXPECT_EQ(0x04, static_cast<uint8_t>(data[0]));
  EXPECT_EQ(0x03, static_cast<uint8_t>(data[1]));
  EXPECT_EQ(0x02, static_cast<uint8_t>(data[2]));
  EXPECT_EQ(0x01, static_cast<uint8_t>(data[3]));
  EXPECT_EQ(0x01020304, ByteOrderUtil::Read<uint32_t>(data.data(), false));
}

TEST(ByteOrderUtilTest, ReadLittleAndBigInterpretBuffersCorrectly) {
  const std::array<char, 4> buffer{char(0x01), char(0x02), char(0x03), char(0x04)};
  EXPECT_EQ(0x01020304, ByteOrderUtil::Read<uint32_t>(buffer.data(), true));
  EXPECT_EQ(0x04030201, ByteOrderUtil::Read<uint32_t>(buffer.data(), false));

  std::array<char, 2> two_bytes{};
  ByteOrderUtil::WriteLittleEndian(two_bytes.data(), static_cast<uint16_t>(0x1234));
  EXPECT_EQ(0x34, static_cast<uint8_t>(two_bytes[0]));
  EXPECT_EQ(0x12, static_cast<uint8_t>(two_bytes[1]));
  EXPECT_EQ(0x1234, ByteOrderUtil::ReadLittleEndian<uint16_t>(two_bytes.data()));
  EXPECT_EQ(0x1234, ByteOrderUtil::Read<uint16_t>(two_bytes.data(), false));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ByteOrderUtilTest.*";
  return RUN_ALL_TESTS();
}
