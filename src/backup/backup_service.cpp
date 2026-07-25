// backup_service.cpp
#include "backup_service.h"

#include <Poco/DateTime.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/RecursiveDirectoryIterator.h>
#include <Poco/Timestamp.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/Zip/Compress.h>
#include <Poco/Zip/ZipCommon.h>
#include <algorithm>
#include <fstream>
#include <rfl/json.hpp>

#include "backup_manifest.hpp"
#include "checksum_utils.hpp"

namespace pcm::backup {
namespace {

struct ScratchGuard {
  std::string path;
  ~ScratchGuard() {
    Poco::File f(path);
    if (f.exists()) {
      try {
        f.remove(true);
      } catch (...) {
      }
    }
  }
};

std::string toRelative(const std::string &base, const std::string &full) {
  std::string rel = full.substr(base.size());
  if (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) {
    rel.erase(0, 1);
  }
  std::replace(rel.begin(), rel.end(), '\\', '/');
  return rel;
}

std::vector<BackupEntry> collectEntries(const std::string &scratchDir) {
  std::vector<BackupEntry> entries;
  Poco::RecursiveDirectoryIterator it(scratchDir);
  const Poco::RecursiveDirectoryIterator end;
  for (; it != end; ++it) {
    if (!it->isFile()) {
      continue;
    }
    BackupEntry entry;
    entry.path = toRelative(scratchDir, it->path());
    entry.size_bytes = static_cast<std::int64_t>(it->getSize());
    entry.sha256 = sha256_file(it->path());
    entries.push_back(std::move(entry));
  }
  return entries;
}

} // namespace

BackupResult BackupService::create_backup(const database::Database &db,
                                          const std::string &destination_path) {
  const auto uuid =
      Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
  const auto scratchDir =
      Poco::Path(Poco::Path::temp()).append("psybackup-" + uuid).toString();
  ScratchGuard guard{scratchDir};
  Poco::File(scratchDir).createDirectories();

  const auto databaseDir =
      Poco::Path(scratchDir).append("database").toString();
  if (!db.export_snapshot(databaseDir)) {
    return {false, "failed to export a consistent database snapshot"};
  }

  BackupManifest manifest;
  manifest.created_at =
      static_cast<std::int64_t>(Poco::Timestamp().epochMicroseconds() / 1000);
  const auto metadata = db.get_application_metadata();
  manifest.workspace_uuid = metadata.workspace_uuid;
  manifest.schema_version = metadata.schema_version;
  manifest.backup_format_version = metadata.backup_format_version;
  manifest.kind = "database";
  manifest.entries = collectEntries(scratchDir);

  const auto manifestPath =
      Poco::Path(scratchDir).append("manifest.json").toString();
  const auto saveResult =
      rfl::json::save(manifestPath, manifest, rfl::json::pretty);
  if (!saveResult) {
    return {false, "failed to write backup manifest: " +
                       saveResult.error().what()};
  }

  const auto tempZipPath = destination_path + ".partial-" + uuid;
  {
    std::ofstream zipOut(tempZipPath, std::ios::binary | std::ios::trunc);
    if (!zipOut) {
      return {false, "failed to open temporary archive for writing"};
    }
    Poco::Zip::Compress compress(zipOut, true);
    compress.addRecursive(Poco::Path(scratchDir),
                          Poco::Zip::ZipCommon::CL_MAXIMUM, true);
    compress.close();
  }

  try {
    Poco::File(tempZipPath).renameTo(destination_path);
  } catch (const Poco::Exception &ex) {
    Poco::File tempZipFile(tempZipPath);
    if (tempZipFile.exists()) {
      tempZipFile.remove();
    }
    return {false,
            std::string("failed to finalize backup: ") + ex.displayText()};
  }

  return {true, {}};
}

} // namespace pcm::backup
