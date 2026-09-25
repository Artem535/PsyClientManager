#include "service/meeting_service.h"

#include "auth/static_token_authorizer.h"
#include "config.h"
#include "db/accounts_repository.h"
#include "db/invitations_repository.h"
#include "db/meetings_repository.h"
#include "db/migrations.h"
#include "db/sqlite_connection.h"

#include <gtest/gtest.h>
#include <sodium.h>

class MeetingServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_EQ(sodium_init() >= 0, true);
    conn = std::make_unique<pcm::tokenbackend::SqliteConnection>(":memory:");
    pcm::tokenbackend::runMigrations(*conn);

    accounts = std::make_unique<pcm::tokenbackend::AccountsRepository>(*conn);
    credential = accounts->seedAccount();
    authorizer = std::make_unique<pcm::tokenbackend::StaticTokenAuthorizer>(*accounts);
    meetings = std::make_unique<pcm::tokenbackend::MeetingsRepository>(*conn);
    invitations = std::make_unique<pcm::tokenbackend::InvitationsRepository>(*conn);

    config.liveKitApiKey = "test-key";
    config.liveKitApiSecret = "test-secret";
    config.tokenTtlSeconds = 600;

    service = std::make_unique<pcm::tokenbackend::MeetingService>(
        *authorizer, *meetings, *invitations, config, "ws://livekit.test:7880");
  }

  std::unique_ptr<pcm::tokenbackend::SqliteConnection> conn;
  std::unique_ptr<pcm::tokenbackend::AccountsRepository> accounts;
  std::unique_ptr<pcm::tokenbackend::Authorizer> authorizer;
  std::unique_ptr<pcm::tokenbackend::MeetingsRepository> meetings;
  std::unique_ptr<pcm::tokenbackend::InvitationsRepository> invitations;
  std::unique_ptr<pcm::tokenbackend::MeetingService> service;
  pcm::tokenbackend::Config config{};
  std::string credential;
};

TEST_F(MeetingServiceTest, CreateMeetingRejectsBadCredential) {
  auto result = service->createMeeting("wrong-credential", "2026-10-01T10:00:00Z",
                                        "2026-10-01T10:50:00Z");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error, pcm::tokenbackend::ServiceError::Unauthorized);
}

TEST_F(MeetingServiceTest, CreateMeetingSucceedsWithGoodCredential) {
  auto result =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.value->meetingRef.empty());
  EXPECT_FALSE(result.value->invitationCode.empty());
  EXPECT_EQ(result.value->passcode.size(), 6u);
}

TEST_F(MeetingServiceTest, SpecialistTokenReturnsSameRoomAsCreatedMeeting) {
  auto created =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(created.ok());

  auto token = service->issueSpecialistToken(credential, created.value->meetingRef);
  ASSERT_TRUE(token.ok());
  EXPECT_FALSE(token.value->jwt.empty());
  EXPECT_EQ(token.value->endpointUrl, "ws://livekit.test:7880");
}

TEST_F(MeetingServiceTest, SpecialistTokenRejectsBadCredential) {
  auto created =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(created.ok());

  auto token = service->issueSpecialistToken("wrong", created.value->meetingRef);
  EXPECT_FALSE(token.ok());
  EXPECT_EQ(token.error, pcm::tokenbackend::ServiceError::Unauthorized);
}

TEST_F(MeetingServiceTest, ClientTokenSucceedsWithCorrectCodeAndPasscode) {
  auto created =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(created.ok());

  auto token =
      service->issueClientToken(created.value->invitationCode, created.value->passcode);
  ASSERT_TRUE(token.ok());
  EXPECT_FALSE(token.value->jwt.empty());
}

TEST_F(MeetingServiceTest, ClientTokenRejectsWrongPasscodeAndCountsAttempt) {
  auto created =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(created.ok());

  auto result = service->issueClientToken(created.value->invitationCode, "000000");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error, pcm::tokenbackend::ServiceError::WrongPasscode);
}

TEST_F(MeetingServiceTest, ClientTokenLocksOutAfterFiveWrongPasscodes) {
  auto created =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(created.ok());

  for (int i = 0; i < 5; ++i) {
    service->issueClientToken(created.value->invitationCode, "000000");
  }

  auto result =
      service->issueClientToken(created.value->invitationCode, created.value->passcode);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error, pcm::tokenbackend::ServiceError::TooManyAttempts);
}

TEST_F(MeetingServiceTest, ClientAndSpecialistTokensShareTheSameRoom) {
  auto created =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(created.ok());

  auto specialistToken = service->issueSpecialistToken(credential, created.value->meetingRef);
  auto clientToken =
      service->issueClientToken(created.value->invitationCode, created.value->passcode);
  ASSERT_TRUE(specialistToken.ok());
  ASSERT_TRUE(clientToken.ok());
  EXPECT_EQ(specialistToken.value->roomName, clientToken.value->roomName);
}

TEST_F(MeetingServiceTest, InvalidateStopsFurtherTokenIssuance) {
  auto created =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(created.ok());

  auto invalidateResult = service->invalidateMeeting(credential, created.value->meetingRef);
  EXPECT_TRUE(invalidateResult.ok());

  auto token = service->issueSpecialistToken(credential, created.value->meetingRef);
  EXPECT_FALSE(token.ok());
  EXPECT_EQ(token.error, pcm::tokenbackend::ServiceError::MeetingWindowClosed);

  auto clientToken =
      service->issueClientToken(created.value->invitationCode, created.value->passcode);
  EXPECT_FALSE(clientToken.ok());
}

TEST_F(MeetingServiceTest, ClientCanReconnectAfterFirstSuccessfulJoin) {
  auto created =
      service->createMeeting(credential, "2026-10-01T10:00:00Z", "2026-10-01T10:50:00Z");
  ASSERT_TRUE(created.ok());

  auto firstJoin =
      service->issueClientToken(created.value->invitationCode, created.value->passcode);
  auto secondJoin =
      service->issueClientToken(created.value->invitationCode, created.value->passcode);

  ASSERT_TRUE(firstJoin.ok());
  ASSERT_TRUE(secondJoin.ok())
      << "invitation code must stay valid for reconnects, not be single-use";
}
