#include "provider_kind.h"

#include <gtest/gtest.h>

TEST(ProviderKindTest, RoundTripsExternalUrl) {
  const auto text = pcm::meeting::providerKindToString(pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(text, "ExternalUrl");
  const auto parsed = pcm::meeting::providerKindFromString(text);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed, pcm::meeting::ProviderKind::ExternalUrl);
}

TEST(ProviderKindTest, RoundTripsLiveKit) {
  const auto text = pcm::meeting::providerKindToString(pcm::meeting::ProviderKind::LiveKit);
  EXPECT_EQ(text, "LiveKit");
  const auto parsed = pcm::meeting::providerKindFromString(text);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed, pcm::meeting::ProviderKind::LiveKit);
}

TEST(ProviderKindTest, UnknownStringParsesToNullopt) {
  EXPECT_FALSE(pcm::meeting::providerKindFromString("SomethingElse").has_value());
  EXPECT_FALSE(pcm::meeting::providerKindFromString("").has_value());
}
