#include <gtest/gtest.h>

#include <memory>

#include "ByteBuffer.hpp"

using rocketmq::ByteBuffer;
using rocketmq::ByteOrder;

namespace {

std::unique_ptr<ByteBuffer> MakeBuffer(int32_t size) {
  return std::unique_ptr<ByteBuffer>(ByteBuffer::allocate(size));
}

}  // namespace

TEST(ByteBufferTest, PutAndGetPrimitivesWithByteOrder) {
  auto buffer = MakeBuffer(64);
  buffer->order(ByteOrder::BO_BIG_ENDIAN);
  buffer->putShort(0x1234).putInt(0x01020304).putLong(0x0102030405060708).putFloat(3.25f).putDouble(6.5);
  buffer->flip();

  EXPECT_EQ(0x1234, buffer->getShort());
  EXPECT_EQ(0x01020304, buffer->getInt());
  EXPECT_EQ(0x0102030405060708LL, buffer->getLong());
  EXPECT_FLOAT_EQ(3.25f, buffer->getFloat());
  EXPECT_DOUBLE_EQ(6.5, buffer->getDouble());

  buffer->clear();
  buffer->order(ByteOrder::BO_LITTLE_ENDIAN);
  buffer->putInt(0x01020304);
  buffer->flip();

  EXPECT_EQ(0x01020304, buffer->order(ByteOrder::BO_LITTLE_ENDIAN).getInt());
  buffer->rewind();
  EXPECT_EQ(0x04030201, buffer->order(ByteOrder::BO_BIG_ENDIAN).getInt());
}

TEST(ByteBufferTest, SliceSharesRemainingBytes) {
  auto buffer = MakeBuffer(10);
  for (int i = 0; i < 10; ++i) {
    buffer->put(static_cast<char>(i));
  }
  buffer->position(2);
  buffer->limit(8);

  std::unique_ptr<ByteBuffer> slice(buffer->slice());
  ASSERT_EQ(6, slice->limit());
  ASSERT_EQ(6, slice->remaining());

  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(static_cast<char>(i + 2), slice->get());
  }

  EXPECT_EQ(2, buffer->position());  // slice reads must not advance parent
}

TEST(ByteBufferTest, CompactMovesUnreadBytesToFront) {
  auto buffer = MakeBuffer(16);
  buffer->put('a').put('b').put('c').put('d');
  buffer->flip();
  EXPECT_EQ('a', buffer->get());
  EXPECT_EQ('b', buffer->get());

  buffer->compact();
  EXPECT_EQ(2, buffer->position());
  EXPECT_EQ(buffer->capacity(), buffer->limit());

  buffer->flip();
  EXPECT_EQ('c', buffer->get());
  EXPECT_EQ('d', buffer->get());
  EXPECT_FALSE(buffer->hasRemaining());
}

TEST(ByteBufferTest, PutFromAnotherBufferConsumesSourcesRemaining) {
  auto src = MakeBuffer(8);
  src->put('x').put('y').flip();

  auto dest = MakeBuffer(8);
  dest->put('a').put('b');
  dest->put(*src);

  dest->flip();
  EXPECT_EQ('a', dest->get());
  EXPECT_EQ('b', dest->get());
  EXPECT_EQ('x', dest->get());
  EXPECT_EQ('y', dest->get());
  EXPECT_FALSE(src->hasRemaining());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ByteBufferTest.*";
  return RUN_ALL_TESTS();
}
