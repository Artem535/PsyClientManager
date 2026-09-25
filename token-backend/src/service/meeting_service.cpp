#include "service/meeting_service.h"

#include "crypto/hashing.h"

#include <chrono>

namespace pcm::tokenbackend {

Result<MeetingService::CreateMeetingOutcome>
MeetingService::createMeeting(const std::string &bearerCredential,
                               const std::string &scheduledStart,
                               const std::string &scheduledEnd) {
  auto accountId = authorizer_.authorize(bearerCredential);
  if (!accountId) {
    return {std::nullopt, ServiceError::Unauthorized};
  }

  auto meeting = meetings_.create(*accountId, scheduledStart, scheduledEnd);
  auto invitation = invitations_.create(meeting.id, *accountId);

  CreateMeetingOutcome outcome;
  outcome.meetingRef = meeting.meetingRef;
  outcome.invitationCode = invitation.invitationCode;
  outcome.passcode = invitation.passcode;
  outcome.scheduledStart = meeting.scheduledStart;
  outcome.scheduledEnd = meeting.scheduledEnd;
  return {outcome, std::nullopt};
}

Result<TokenResult> MeetingService::issueSpecialistToken(const std::string &bearerCredential,
                                                           const std::string &meetingRef) {
  auto accountId = authorizer_.authorize(bearerCredential);
  if (!accountId) {
    return {std::nullopt, ServiceError::Unauthorized};
  }

  auto meeting = meetings_.findByRef(meetingRef);
  if (!meeting || meeting->accountId != *accountId) {
    return {std::nullopt, ServiceError::NotFound};
  }
  if (meeting->status != "active") {
    return {std::nullopt, ServiceError::MeetingWindowClosed};
  }

  VideoGrants grants;
  grants.room = meeting->roomName;
  std::string identity = "practitioner-" + meeting->meetingRef;
  auto jwt = mintLiveKitJwt(config_.liveKitApiKey, config_.liveKitApiSecret, identity, grants,
                             config_.tokenTtlSeconds);

  auto now = std::chrono::system_clock::now();
  auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

  TokenResult result;
  result.endpointUrl = liveKitEndpointUrl_;
  result.roomName = meeting->roomName;
  result.jwt = jwt;
  result.expiresAtUnix = nowSeconds + config_.tokenTtlSeconds;
  return {result, std::nullopt};
}

Result<TokenResult> MeetingService::issueClientToken(const std::string &invitationCode,
                                                       const std::string &passcode) {
  auto invitation = invitations_.findByCode(invitationCode);
  if (!invitation) {
    return {std::nullopt, ServiceError::NotFound};
  }
  if (invitation->status != "active") {
    // The only way an invitation's status leaves "active" today is via
    // InvitationsRepository::recordFailedPasscodeAttempt auto-invalidating it
    // after kMaxPasscodeAttempts wrong guesses, so a non-active invitation
    // here means the passcode attempt budget is exhausted.
    return {std::nullopt, ServiceError::TooManyAttempts};
  }

  if (!passcodeMatches(passcode, invitation->passcodeHash)) {
    int attempts = invitations_.recordFailedPasscodeAttempt(invitation->id);
    if (attempts >= 5) {
      return {std::nullopt, ServiceError::TooManyAttempts};
    }
    return {std::nullopt, ServiceError::WrongPasscode};
  }

  auto meeting = meetings_.findById(invitation->meetingId);
  if (!meeting || meeting->status != "active") {
    return {std::nullopt, ServiceError::MeetingWindowClosed};
  }

  VideoGrants grants;
  grants.room = meeting->roomName;
  std::string identity = "client-" + meeting->meetingRef;
  auto jwt = mintLiveKitJwt(config_.liveKitApiKey, config_.liveKitApiSecret, identity, grants,
                             config_.tokenTtlSeconds);

  auto now = std::chrono::system_clock::now();
  auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

  TokenResult result;
  result.endpointUrl = liveKitEndpointUrl_;
  result.roomName = meeting->roomName;
  result.jwt = jwt;
  result.expiresAtUnix = nowSeconds + config_.tokenTtlSeconds;
  return {result, std::nullopt};
}

Result<std::monostate> MeetingService::invalidateMeeting(const std::string &bearerCredential,
                                                           const std::string &meetingRef) {
  auto accountId = authorizer_.authorize(bearerCredential);
  if (!accountId) {
    return {std::nullopt, ServiceError::Unauthorized};
  }

  auto meeting = meetings_.findByRef(meetingRef);
  if (!meeting || meeting->accountId != *accountId) {
    return {std::nullopt, ServiceError::NotFound};
  }

  meetings_.invalidate(meeting->id);
  return {std::monostate{}, std::nullopt};
}

} // namespace pcm::tokenbackend
