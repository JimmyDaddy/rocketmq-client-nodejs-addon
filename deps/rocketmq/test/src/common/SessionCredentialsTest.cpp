#include <gtest/gtest.h>

#include "SessionCredentials.h"

using rocketmq::SessionCredentials;

TEST(SessionCredentialsTest, DefaultsAreInvalid) {
  SessionCredentials creds;
  EXPECT_FALSE(creds.isValid());
  EXPECT_EQ("", creds.access_key());
  EXPECT_EQ("", creds.secret_key());
  EXPECT_EQ("ALIYUN", creds.auth_channel());
}

TEST(SessionCredentialsTest, SettersEnableValidity) {
  SessionCredentials creds;
  creds.set_access_key("ak");
  creds.set_secret_key("sk");
  creds.set_auth_channel("channel");
  creds.set_signature("sig");
  creds.set_signature_method("HmacSHA1");

  EXPECT_TRUE(creds.isValid());
  EXPECT_EQ("ak", creds.access_key());
  EXPECT_EQ("sk", creds.secret_key());
  EXPECT_EQ("channel", creds.auth_channel());
  EXPECT_EQ("sig", creds.signature());
  EXPECT_EQ("HmacSHA1", creds.signature_method());
}

TEST(SessionCredentialsTest, CopyConstructorCopiesAllFields) {
  SessionCredentials original("ak", "sk", "chan");
  original.set_signature("sig");
  original.set_signature_method("method");

  SessionCredentials copy(original);
  EXPECT_EQ("ak", copy.access_key());
  EXPECT_EQ("sk", copy.secret_key());
  EXPECT_EQ("sig", copy.signature());
  EXPECT_EQ("method", copy.signature_method());
  EXPECT_EQ("chan", copy.auth_channel());
  EXPECT_TRUE(copy.isValid());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  testing::GTEST_FLAG(throw_on_failure) = true;
  testing::GTEST_FLAG(filter) = "SessionCredentialsTest.*";
  return RUN_ALL_TESTS();
}
