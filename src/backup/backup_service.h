// backup_service.h
#pragma once

#include <string>

#include "database.h"

namespace pcm::backup {

struct BackupResult {
  bool ok = false;
  std::string error;
};

class BackupService {
public:
  BackupResult create_backup(const database::Database &db,
                              const std::string &destination_path);
};

} // namespace pcm::backup
