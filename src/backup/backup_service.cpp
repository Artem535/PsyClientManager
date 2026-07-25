// backup_service.cpp
#include "backup_service.h"

namespace pcm::backup {

BackupResult BackupService::create_backup(const database::Database &db,
                                          const std::string &destination_path) {
  (void)db;
  (void)destination_path;
  return {false, "not implemented"};
}

} // namespace pcm::backup
