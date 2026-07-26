// backup_service.h
#pragma once

#include <optional>
#include <string>

#include "database.h"

namespace pcm::backup {

struct BackupOptions {
  std::optional<std::string> attachments_root;
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
