#include "config.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdlib>
#include <stdexcept>
#include <string>

class ConfigTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Save current env vars
    const char *apiKey = std::getenv("LIVEKIT_API_KEY");
    savedApiKey_ = apiKey ? apiKey : "";
    hasApiKey_ = apiKey != nullptr;

    const char *apiSecret = std::getenv("LIVEKIT_API_SECRET");
    savedApiSecret_ = apiSecret ? apiSecret : "";
    hasApiSecret_ = apiSecret != nullptr;

    const char *port = std::getenv("PORT");
    savedPort_ = port ? port : "";
    hasPort_ = port != nullptr;

    const char *dbPath = std::getenv("DB_PATH");
    savedDbPath_ = dbPath ? dbPath : "";
    hasDbPath_ = dbPath != nullptr;

    const char *ttl = std::getenv("TOKEN_TTL_SECONDS");
    savedTtl_ = ttl ? ttl : "";
    hasTtl_ = ttl != nullptr;
  }

  void TearDown() override {
    // Restore env vars to their original state
    if (hasApiKey_) {
      setenv("LIVEKIT_API_KEY", savedApiKey_.c_str(), 1);
    } else {
      unsetenv("LIVEKIT_API_KEY");
    }

    if (hasApiSecret_) {
      setenv("LIVEKIT_API_SECRET", savedApiSecret_.c_str(), 1);
    } else {
      unsetenv("LIVEKIT_API_SECRET");
    }

    if (hasPort_) {
      setenv("PORT", savedPort_.c_str(), 1);
    } else {
      unsetenv("PORT");
    }

    if (hasDbPath_) {
      setenv("DB_PATH", savedDbPath_.c_str(), 1);
    } else {
      unsetenv("DB_PATH");
    }

    if (hasTtl_) {
      setenv("TOKEN_TTL_SECONDS", savedTtl_.c_str(), 1);
    } else {
      unsetenv("TOKEN_TTL_SECONDS");
    }
  }

private:
  std::string savedApiKey_;
  bool hasApiKey_ = false;
  std::string savedApiSecret_;
  bool hasApiSecret_ = false;
  std::string savedPort_;
  bool hasPort_ = false;
  std::string savedDbPath_;
  bool hasDbPath_ = false;
  std::string savedTtl_;
  bool hasTtl_ = false;
};

TEST_F(ConfigTest, ThrowsWhenLiveKitApiKeyMissing) {
  unsetenv("LIVEKIT_API_KEY");
  setenv("LIVEKIT_API_SECRET", "test-secret", 1);

  EXPECT_THROW(
      {
        try {
          pcm::tokenbackend::Config::fromEnv();
          FAIL() << "Expected std::runtime_error";
        } catch (const std::runtime_error &e) {
          EXPECT_THAT(std::string(e.what()), ::testing::HasSubstr("LIVEKIT_API_KEY"));
          throw;
        }
      },
      std::runtime_error);
}

TEST_F(ConfigTest, ThrowsWhenLiveKitApiSecretMissing) {
  setenv("LIVEKIT_API_KEY", "test-key", 1);
  unsetenv("LIVEKIT_API_SECRET");

  EXPECT_THROW(
      {
        try {
          pcm::tokenbackend::Config::fromEnv();
          FAIL() << "Expected std::runtime_error";
        } catch (const std::runtime_error &e) {
          EXPECT_THAT(std::string(e.what()), ::testing::HasSubstr("LIVEKIT_API_SECRET"));
          throw;
        }
      },
      std::runtime_error);
}

TEST_F(ConfigTest, SucceedsWithRequiredVarsAndAppliesDefaults) {
  setenv("LIVEKIT_API_KEY", "test-key", 1);
  setenv("LIVEKIT_API_SECRET", "test-secret", 1);
  unsetenv("PORT");
  unsetenv("DB_PATH");
  unsetenv("TOKEN_TTL_SECONDS");

  pcm::tokenbackend::Config config = pcm::tokenbackend::Config::fromEnv();

  EXPECT_EQ(config.liveKitApiKey, "test-key");
  EXPECT_EQ(config.liveKitApiSecret, "test-secret");
  EXPECT_EQ(config.port, 8080);
  EXPECT_EQ(config.dbPath, "token-backend.sqlite3");
  EXPECT_EQ(config.tokenTtlSeconds, 600);
}

TEST_F(ConfigTest, SucceedsWithAllVarsSet) {
  setenv("LIVEKIT_API_KEY", "my-key", 1);
  setenv("LIVEKIT_API_SECRET", "my-secret", 1);
  setenv("PORT", "9000", 1);
  setenv("DB_PATH", "/tmp/custom.db", 1);
  setenv("TOKEN_TTL_SECONDS", "3600", 1);

  pcm::tokenbackend::Config config = pcm::tokenbackend::Config::fromEnv();

  EXPECT_EQ(config.liveKitApiKey, "my-key");
  EXPECT_EQ(config.liveKitApiSecret, "my-secret");
  EXPECT_EQ(config.port, 9000);
  EXPECT_EQ(config.dbPath, "/tmp/custom.db");
  EXPECT_EQ(config.tokenTtlSeconds, 3600);
}
