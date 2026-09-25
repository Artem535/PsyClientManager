#pragma once

#include "db/sqlite_connection.h"

#include <cstdint>
#include <optional>
#include <string>

namespace pcm::tokenbackend {

using AccountId = int64_t;

class AccountsRepository {
public:
  explicit AccountsRepository(SqliteConnection &conn) : conn_(conn) {}

  std::string seedAccount();
  std::optional<AccountId> findByCredential(const std::string &rawCredential);

private:
  SqliteConnection &conn_;
};

} // namespace pcm::tokenbackend
