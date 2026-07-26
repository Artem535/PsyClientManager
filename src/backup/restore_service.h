#pragma once

#include <optional>
#include <string>

namespace pcm::backup {

struct RestoreOptions {
  std::optional<std::string> attachments_root;
};

struct RestoreResult {
  bool ok = false;
  std::string error;
  std::string protective_database_path;
  std::string protective_attachments_path;
};

class RestoreService {
public:
  RestoreResult restore_backup(const std::string &backup_path,
                               const std::string &database_root,
                               const RestoreOptions &options = {});
};

} // namespace pcm::backup
