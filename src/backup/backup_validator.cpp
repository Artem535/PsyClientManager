// backup_validator.cpp
#include "backup_validator.h"

#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/RecursiveDirectoryIterator.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/Zip/Decompress.h>
#include <algorithm>
#include <fstream>
#include <rfl/json.hpp>
#include <set>

#include "backup_manifest.hpp"
#include "checksum_utils.hpp"

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

std::string toRelative(const std::string &base, const std::string &full) {
  std::string rel = full.substr(base.size());
  if (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) {
    rel.erase(0, 1);
  }
  std::replace(rel.begin(), rel.end(), '\\', '/');
  return rel;
}

} // namespace

ValidationResult BackupValidator::validate(const std::string &backup_path) {
  ValidationResult result;

  try {
    Poco::File backupFile(backup_path);
    if (!backupFile.exists() || !backupFile.isFile()) {
      result.errors.push_back("backup file does not exist: " + backup_path);
      return result;
    }

    const auto uuid =
        Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
    const auto extractDir = Poco::Path(Poco::Path::temp())
                                .append("psybackup-validate-" + uuid)
                                .toString();
    ScratchGuard guard{extractDir};

    std::ifstream zipIn(backup_path, std::ios::binary);
    if (!zipIn) {
      result.errors.push_back("cannot open backup file: " + backup_path);
      return result;
    }

    Poco::Zip::Decompress decompress(zipIn, Poco::Path(extractDir));
    decompress.decompressAllFiles();

    const auto manifestPath =
        Poco::Path(extractDir).append("manifest.json").toString();
    if (!Poco::File(manifestPath).exists()) {
      result.errors.push_back("backup is missing manifest.json");
      return result;
    }

    const auto manifestResult = rfl::json::load<BackupManifest>(manifestPath);
    if (!manifestResult) {
      result.errors.push_back("failed to parse manifest.json: " +
                              manifestResult.error().what());
      return result;
    }
    const auto &manifest = manifestResult.value();

    if (manifest.psybackup_format_version != 1) {
      result.errors.push_back(
          "unsupported psybackup_format_version: " +
          std::to_string(manifest.psybackup_format_version));
      return result;
    }

    for (const auto &entry : manifest.entries) {
      Poco::Path entryPath(extractDir);
      entryPath.append(Poco::Path(entry.path, Poco::Path::PATH_UNIX));
      Poco::File entryFile(entryPath);
      if (!entryFile.exists() || !entryFile.isFile()) {
        result.errors.push_back("missing entry: " + entry.path);
        continue;
      }
      if (static_cast<std::int64_t>(entryFile.getSize()) != entry.size_bytes) {
        result.errors.push_back("size mismatch for entry: " + entry.path);
        continue;
      }
      const auto actualHash = sha256_file(entryPath.toString());
      if (actualHash != entry.sha256) {
        result.errors.push_back("checksum mismatch for entry: " + entry.path);
      }
    }

    std::set<std::string> knownPaths;
    for (const auto &entry : manifest.entries) {
      knownPaths.insert(entry.path);
    }

    Poco::RecursiveDirectoryIterator extractedIt(extractDir);
    const Poco::RecursiveDirectoryIterator extractedEnd;
    for (; extractedIt != extractedEnd; ++extractedIt) {
      if (!extractedIt->isFile()) {
        continue;
      }
      const auto rel = toRelative(extractDir, extractedIt->path());
      if (rel == "manifest.json") {
        continue;
      }
      if (!knownPaths.contains(rel)) {
        result.errors.push_back("unexpected file not listed in manifest: " + rel);
      }
    }

    result.ok = result.errors.empty();
    return result;
  } catch (const Poco::Exception &ex) {
    result.errors.push_back("validation failed: " + ex.displayText());
    return result;
  } catch (const std::exception &ex) {
    result.errors.push_back("validation failed: " + std::string(ex.what()));
    return result;
  }
}

} // namespace pcm::backup
