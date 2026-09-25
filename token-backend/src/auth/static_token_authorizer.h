#pragma once

#include "auth/authorizer.h"
#include "db/accounts_repository.h"

namespace pcm::tokenbackend {

class StaticTokenAuthorizer : public Authorizer {
public:
  explicit StaticTokenAuthorizer(AccountsRepository &accounts) : accounts_(accounts) {}
  std::optional<AccountId> authorize(const std::string &bearerCredential) override {
    return accounts_.findByCredential(bearerCredential);
  }

private:
  AccountsRepository &accounts_;
};

} // namespace pcm::tokenbackend
