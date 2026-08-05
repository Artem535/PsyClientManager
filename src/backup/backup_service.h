// backup_service.h
#pragma once

#include <optional>
#include <string>

#include "database.h"
#include "encrypted_container.h"

namespace pcm::backup {

struct BackupEncryptionOptions {
  std::optional<MasterKey> master_key;
  std::optional<std::string> recovery_password;
  std::optional<RecoveryEnvelope> recovery_envelope;
};

struct BackupOptions {
  std::optional<std::string> attachments_root;
  std::optional<BackupEncryptionOptions> encryption;
};

struct BackupResult {
  bool ok = false;
  std::string error;
};

class BackupService {
public:
  BackupResult create_backup(const database::Database &db,
                              const std::string &destination_path,
                              const BackupOptions &options = {});
};

} // namespace pcm::backup
