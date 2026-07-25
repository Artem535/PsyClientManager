// backup_service.cpp
#include "backup_service.h"

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

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace pcm::backup {
namespace {

struct ScratchGuard {
  std::string path;
  ~ScratchGuard() {
    try {
      Poco::File f(path);
      if (f.exists()) {
        f.remove(true);
      }
    } catch (...) {
    }
  }
};

struct TempFileGuard {
  std::string path;
  ~TempFileGuard() {
    try {
      Poco::File f(path);
      if (f.exists()) {
        f.remove();
      }
    } catch (...) {
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
                                          const std::string &destination_path,
                                          const BackupOptions &options) {
  try {
    const auto uuid =
        Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
    const auto scratchDir =
        Poco::Path(Poco::Path::temp()).append("psybackup-" + uuid).toString();
    ScratchGuard guard{scratchDir};
    Poco::File(scratchDir).createDirectories();
#ifndef _WIN32
    // Restrict the scratch directory to the current user: it holds a
    // plaintext export of client PII for the duration of create_backup.
    // TODO: Windows ACL hardening is a separate follow-up.
    ::chmod(scratchDir.c_str(), S_IRWXU);
#endif

    const auto databaseDir =
        Poco::Path(scratchDir).append("database").toString();
    if (!db.export_snapshot(databaseDir)) {
      return {false, "failed to export a consistent database snapshot"};
    }

    std::string kind = "database";
    if (options.attachments_root.has_value()) {
      Poco::File attachmentsRootFile(*options.attachments_root);
      if (!attachmentsRootFile.exists() || !attachmentsRootFile.isDirectory()) {
        return {false,
                "attachments_root does not exist or is not a directory: " +
                    *options.attachments_root};
      }
      const auto attachmentsDir =
          Poco::Path(scratchDir).append("attachments").toString();
      attachmentsRootFile.copyTo(attachmentsDir);
      kind = "database_and_attachments";
    }

    BackupManifest manifest;
    manifest.created_at = static_cast<std::int64_t>(
        Poco::Timestamp().epochMicroseconds() / 1000);
    const auto metadata = db.get_application_metadata();
    manifest.workspace_uuid = metadata.workspace_uuid;
    manifest.schema_version = metadata.schema_version;
    manifest.backup_format_version = metadata.backup_format_version;
    manifest.kind = kind;
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
    TempFileGuard zipGuard{tempZipPath};
    {
      std::ofstream zipOut(tempZipPath, std::ios::binary | std::ios::trunc);
      if (!zipOut) {
        return {false, "failed to open temporary archive for writing"};
      }
      Poco::Zip::Compress compress(zipOut, true);
      compress.addRecursive(Poco::Path(scratchDir),
                            Poco::Zip::ZipCommon::CL_MAXIMUM, true);
      compress.close();
      zipOut.flush();
      if (!zipOut) {
        return {false, "failed to write temporary archive"};
      }
      zipOut.close();
      if (!zipOut) {
        return {false, "failed to finalize temporary archive"};
      }
    }

    Poco::File(tempZipPath).renameTo(destination_path);

    return {true, {}};
  } catch (const Poco::Exception &ex) {
    return {false, std::string("backup failed: ") + ex.displayText()};
  } catch (const std::exception &ex) {
    return {false, std::string("backup failed: ") + ex.what()};
  }
}

} // namespace pcm::backup
