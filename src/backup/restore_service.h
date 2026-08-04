#pragma once

#include <optional>
#include <string>

namespace pcm::backup {

struct RestoreOptions {
  std::optional<std::string> attachments_root;
  std::optional<std::string> recovery_password;
};

struct RestoreResult {
  bool ok = false;
  std::string error;
  std::string protective_database_path;
  std::string protective_attachments_path;
};

struct PendingRestoreMarker {
  std::string backup_path;
};

bool write_pending_restore_marker(const std::string &marker_path,
                                   const std::string &backup_path);
std::optional<PendingRestoreMarker>
read_pending_restore_marker(const std::string &marker_path);
void remove_pending_restore_marker(const std::string &marker_path);

class RestoreService {
public:
  RestoreResult restore_backup(const std::string &backup_path,
                               const std::string &database_root,
                               const RestoreOptions &options = {});
};

} // namespace pcm::backup
