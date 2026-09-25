#include "auth/static_token_authorizer.h"
#include "db/accounts_repository.h"
#include "db/migrations.h"
#include "db/sqlite_connection.h"

#include <gtest/gtest.h>
#include <sodium.h>

class StaticTokenAuthorizerTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_EQ(sodium_init() >= 0, true);
    conn = std::make_unique<pcm::tokenbackend::SqliteConnection>(":memory:");
    pcm::tokenbackend::runMigrations(*conn);
    accounts = std::make_unique<pcm::tokenbackend::AccountsRepository>(*conn);
    credential = accounts->seedAccount();
  }
  std::unique_ptr<pcm::tokenbackend::SqliteConnection> conn;
  std::unique_ptr<pcm::tokenbackend::AccountsRepository> accounts;
  std::string credential;
};

TEST_F(StaticTokenAuthorizerTest, AuthorizesValidCredential) {
  pcm::tokenbackend::StaticTokenAuthorizer authorizer(*accounts);
  EXPECT_TRUE(authorizer.authorize(credential).has_value());
}

TEST_F(StaticTokenAuthorizerTest, RejectsInvalidCredential) {
  pcm::tokenbackend::StaticTokenAuthorizer authorizer(*accounts);
  EXPECT_FALSE(authorizer.authorize("garbage").has_value());
}
