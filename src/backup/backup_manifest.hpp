// backup_manifest.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcm::backup {

struct BackupEntry {
  std::string path;
  std::int64_t size_bytes = 0;
  std::string sha256;
};

struct BackupManifest {
  std::int32_t psybackup_format_version = 1;
  std::int64_t created_at = 0;
  std::string workspace_uuid;
  std::int32_t schema_version = 0;
  std::int32_t backup_format_version = 0;
  std::string kind; // "database" or "database_and_attachments"
  std::vector<BackupEntry> entries;
};

} // namespace pcm::backup
