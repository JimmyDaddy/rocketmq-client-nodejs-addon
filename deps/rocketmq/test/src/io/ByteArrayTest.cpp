#include <gtest/gtest.h>

#include <string>

#include "ByteArray.h"

using rocketmq::ByteArrayRef;
using rocketmq::batos;
using rocketmq::catoba;
using rocketmq::slice;
using rocketmq::stoba;

TEST(ByteArrayTest, SliceSharesBackingBuffer) {
  auto original = stoba("abcdef");
  auto window = slice(original, 2, 3);

  ASSERT_EQ(3u, window->size());
  EXPECT_EQ('c', (*window)[0]);
  EXPECT_EQ('d', (*window)[1]);
  EXPECT_EQ('e', (*window)[2]);

  (*window)[1] = 'X';
  EXPECT_EQ('X', (*original)[3]);
  EXPECT_EQ("abcXef", batos(original));
}

TEST(ByteArrayTest, CatobaMatchesPointerContents) {
  const char payload[] = {'h', 'i', '!', '\0'};
  auto array = catoba(payload, 3);
  ASSERT_EQ(3u, array->size());
  EXPECT_EQ('h', (*array)[0]);
  EXPECT_EQ('i', (*array)[1]);
  EXPECT_EQ('!', (*array)[2]);
  EXPECT_EQ("hi!", batos(array));
}

TEST(ByteArrayTest, StobaRoundTripsStrings) {
  std::string text = "rocketmq";
  auto arr = stoba(text);
  ASSERT_EQ(text.size(), arr->size());
  EXPECT_EQ(text, batos(arr));

  auto arr_move = stoba(std::string("buffer"));
  EXPECT_EQ("buffer", batos(arr_move));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "ByteArrayTest.*";
  return RUN_ALL_TESTS();
}
