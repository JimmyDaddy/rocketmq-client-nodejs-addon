#include <gtest/gtest.h>

#include <cstring>

#include "c/CSendResult.h"

namespace {

TEST(CSendResultTest, DefaultInitializationZeroesFields) {
  CSendResult result{};
  EXPECT_EQ(E_SEND_OK, result.sendStatus);
  EXPECT_EQ(0, result.offset);
  EXPECT_EQ('\0', result.msgId[0]);
}

TEST(CSendResultTest, AcceptsStatusAssignments) {
  CSendResult result{};
  result.sendStatus = E_SEND_SLAVE_NOT_AVAILABLE;
  EXPECT_EQ(E_SEND_SLAVE_NOT_AVAILABLE, result.sendStatus);

  result.sendStatus = E_SEND_FLUSH_DISK_TIMEOUT;
  EXPECT_EQ(E_SEND_FLUSH_DISK_TIMEOUT, result.sendStatus);
}

TEST(CSendResultTest, StoresMessageIdentifiersAndOffsets) {
  CSendResult result{};
  std::strncpy(result.msgId, "00ABCDEF1234", sizeof(result.msgId) - 1);
  result.offset = 4096;

  EXPECT_STREQ("00ABCDEF1234", result.msgId);
  EXPECT_EQ(4096, result.offset);
}

}  // namespace
