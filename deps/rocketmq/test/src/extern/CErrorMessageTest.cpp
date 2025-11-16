#include <gtest/gtest.h>

#include <string>

#include "CErrorContainer.h"
#include "c/CErrorMessage.h"

namespace {

using rocketmq::CErrorContainer;

class CErrorMessageTest : public ::testing::Test {
 protected:
  void SetUp() override { CErrorContainer::setErrorMessage(""); }
  void TearDown() override { CErrorContainer::setErrorMessage(""); }
};

TEST_F(CErrorMessageTest, ReturnsEmptyStringByDefault) {
  EXPECT_STREQ("", GetLatestErrorMessage());
}

TEST_F(CErrorMessageTest, ReflectsLatestMessageFromContainer) {
  const std::string first_error = "network-error";
  CErrorContainer::setErrorMessage(first_error);
  EXPECT_STREQ(first_error.c_str(), GetLatestErrorMessage());

  const std::string second_error = "timeout";
  CErrorContainer::setErrorMessage(second_error);
  EXPECT_STREQ(second_error.c_str(), GetLatestErrorMessage());
}

TEST_F(CErrorMessageTest, SupportsMoveAssignedMessages) {
  std::string buffered_error = "temporary-buffer-overflow";
  CErrorContainer::setErrorMessage(std::move(buffered_error));
  EXPECT_STREQ("temporary-buffer-overflow", GetLatestErrorMessage());
}

}  // namespace
