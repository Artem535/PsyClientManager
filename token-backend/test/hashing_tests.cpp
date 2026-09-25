#include "crypto/hashing.h"
#include "crypto/random_token.h"

#include <gtest/gtest.h>
#include <sodium.h>

class CryptoTest : public ::testing::Test {
protected:
  void SetUp() override { ASSERT_EQ(sodium_init() >= 0, true); }
};

TEST_F(CryptoTest, GenerateUrlSafeTokenHasNoPaddingOrSlashes) {
  auto token = pcm::tokenbackend::generateUrlSafeToken(24);
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(token.find('='), std::string::npos);
  EXPECT_EQ(token.find('/'), std::string::npos);
  EXPECT_EQ(token.find('+'), std::string::npos);
}

TEST_F(CryptoTest, GenerateNumericPasscodeIsSixDigits) {
  auto passcode = pcm::tokenbackend::generateNumericPasscode();
  ASSERT_EQ(passcode.size(), 6u);
  for (char c : passcode) {
    EXPECT_TRUE(c >= '0' && c <= '9');
  }
}

TEST_F(CryptoTest, FastHashIsDeterministic) {
  auto a = pcm::tokenbackend::fastHash("same-input");
  auto b = pcm::tokenbackend::fastHash("same-input");
  EXPECT_EQ(a, b);
}

TEST_F(CryptoTest, FastHashMatchesVerifiesCorrectly) {
  auto hash = pcm::tokenbackend::fastHash("token-value");
  EXPECT_TRUE(pcm::tokenbackend::fastHashMatches("token-value", hash));
  EXPECT_FALSE(pcm::tokenbackend::fastHashMatches("wrong-value", hash));
}

TEST_F(CryptoTest, PasscodeHashRoundTrips) {
  auto hash = pcm::tokenbackend::hashPasscode("123456");
  EXPECT_TRUE(pcm::tokenbackend::passcodeMatches("123456", hash));
  EXPECT_FALSE(pcm::tokenbackend::passcodeMatches("000000", hash));
}

TEST_F(CryptoTest, PasscodeHashIsRandomizedPerCall) {
  auto first = pcm::tokenbackend::hashPasscode("123456");
  auto second = pcm::tokenbackend::hashPasscode("123456");
  EXPECT_NE(first, second) << "Argon2id must salt each call independently";
}
