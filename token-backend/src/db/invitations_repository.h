#pragma once

#include "db/accounts_repository.h"
#include "db/sqlite_connection.h"

#include <optional>
#include <string>

namespace pcm::tokenbackend {

struct Invitation {
  int64_t id;
  int64_t meetingId;
  AccountId accountId;
  std::string passcodeHash;
  int passcodeAttempts;
  std::string status;
};

class InvitationsRepository {
public:
  explicit InvitationsRepository(SqliteConnection &conn) : conn_(conn) {}

  struct CreateResult {
    std::string invitationCode;
    std::string passcode;
    Invitation invitation;
  };

  CreateResult create(int64_t meetingId, AccountId accountId);
  std::optional<Invitation> findByCode(const std::string &invitationCode);
  int recordFailedPasscodeAttempt(int64_t invitationId);
  void invalidate(int64_t invitationId);
  // Retires every invitation currently attached to a meeting. Used by the
  // re-issue path so the superseded code stops working the moment a fresh
  // one is minted.
  void invalidateAllForMeeting(int64_t meetingId);

private:
  SqliteConnection &conn_;
};

} // namespace pcm::tokenbackend
