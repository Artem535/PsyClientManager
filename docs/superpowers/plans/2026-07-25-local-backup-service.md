# Local Backup Service Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `BackupService` that writes a local `.psybackup` zip archive (consistent DuckDB snapshot + optional attachments + checksummed manifest) to an explicit destination path atomically, plus a `BackupValidator` that verifies an existing `.psybackup` file's integrity.

**Architecture:** A new Qt-free module `src/backup/` (mirrors `src/database`/`src/config`) adds one method to `Database` (`export_snapshot`, wrapping DuckDB's `EXPORT DATABASE ... (FORMAT PARQUET)`), then assembles a scratch directory (export + optional attachments + `manifest.json` with per-file SHA-256), zips it with `Poco::Zip::Compress`, and finalizes with a same-directory rename so the destination is only ever touched once, atomically, on success.

**Tech Stack:** C++20, DuckDB (vendored, 1.4.4), Poco (Foundation + newly-enabled `Zip` feature), reflect-cpp (`rfl::json`), GoogleTest.

Spec: `docs/superpowers/specs/2026-07-25-local-backup-service-design.md`

## Global Constraints

- Format is a real ZIP file (not a bare directory), built with `Poco::Zip::Compress`/`Decompress` — enable Poco's vcpkg `zip` feature; no other new third-party dependency.
- DuckDB snapshot must go through `EXPORT DATABASE '<dir>' (FORMAT PARQUET);` on a fresh `duckdb::Connection` — never copy the live `.db`/WAL files directly.
- Checksums are SHA-256 via `Poco::SHA2Engine` (Poco Foundation, no OpenSSL needed).
- `manifest.json` is serialized via `rfl::json` (reflect-cpp is already a project dependency; JSON support needs no extra vcpkg feature — only `yaml` was previously enabled).
- `BackupService`/`BackupValidator` live in `src/backup/`, are Qt-free, and are tested with plain GoogleTest (same pattern as `test/database_tests.cpp`), not linked into the Qt app target (no UI in this issue's scope).
- `destination_path` is always explicit and caller-supplied; `BackupService` never creates the destination's parent directory.
- The destination file is written via: build in scratch dir → zip into `destination_path + ".partial-<uuid>"` (same directory as the destination) → `Poco::File::renameTo(destination_path)`. On any failure before that final rename, `destination_path` must be left byte-for-byte unchanged and no partial file must remain.
- Out of scope: encryption, cloud upload, restore, restore UI.

---

### Task 1: `Database::export_snapshot` — consistent DuckDB snapshot export

**Files:**
- Modify: `src/database/database.h`
- Modify: `src/database/database.cpp`
- Test: `test/database_tests.cpp`

**Interfaces:**
- Produces: `bool pcm::database::Database::export_snapshot(const std::string &target_dir) const;` — runs `EXPORT DATABASE '<target_dir>' (FORMAT PARQUET);` on a fresh connection; returns `true` on success, `false` (and logs via `PLOG_ERROR`) on failure. Creates `target_dir` itself (DuckDB does this).

- [ ] **Step 1: Write the failing test**

Add to `test/database_tests.cpp` (after the existing `PersistsApplicationMetadataAcrossRestart` test):

```cpp
TEST(DatabaseTest, ExportSnapshotWritesConsistentParquetFiles) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_export")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Export"};
  client.last_name = std::string{"Test"};
  ASSERT_GT(db.add_client(client), 0);

  const auto exportDir =
      Poco::Path(Poco::Path::current()).append("tmp_dir_export_snapshot").toString();
  Poco::File exportDirFile(exportDir);
  if (exportDirFile.exists()) {
    exportDirFile.remove(true);
  }

  ASSERT_TRUE(db.export_snapshot(exportDir));

  EXPECT_TRUE(Poco::File(Poco::Path(exportDir).append("schema.sql")).exists());
  EXPECT_TRUE(Poco::File(Poco::Path(exportDir).append("load.sql")).exists());
  EXPECT_TRUE(Poco::File(Poco::Path(exportDir).append("Client.parquet")).exists());

  Poco::File(exportDir).remove(true);
  db_dir.remove(true);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target PsyClientManager_database_tests`
Expected: FAIL to compile — `'class pcm::database::Database' has no member named 'export_snapshot'`

- [ ] **Step 3: Declare the method**

In `src/database/database.h`, add this line right after `DuckApplicationMetadata get_application_metadata();` (still inside the `public:` section):

```cpp
  bool export_snapshot(const std::string &target_dir) const;
```

- [ ] **Step 4: Implement the method**

In `src/database/database.cpp`, insert this new function right after the closing brace of `Database::get_application_metadata()` and before the `// --- Init ---` comment:

```cpp
bool Database::export_snapshot(const std::string &target_dir) const {
  duckdb::Connection conn(*mDb);
  std::string escaped_dir;
  escaped_dir.reserve(target_dir.size());
  for (const char ch : target_dir) {
    if (ch == '\'') {
      escaped_dir += "''";
    } else {
      escaped_dir += ch;
    }
  }

  const auto query =
      "EXPORT DATABASE '" + escaped_dir + "' (FORMAT PARQUET);";
  auto result = conn.Query(query);
  if (!result || result->HasError()) {
    PLOG_ERROR << "export_snapshot failed: "
               << (result ? result->GetError() : std::string{"null result"});
    return false;
  }
  return true;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build --target PsyClientManager_database_tests && ctest --test-dir build -R ExportSnapshotWritesConsistentParquetFiles --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/database/database.h src/database/database.cpp test/database_tests.cpp
git commit -m "feat(database): add export_snapshot for consistent DuckDB backups"
```

---

### Task 2: `src/backup` module scaffold — database-only `BackupService`

**Files:**
- Modify: `vcpkg.json`
- Modify: `CMakeLists.txt`
- Modify: `test/CMakeLists.txt`
- Create: `src/backup/CMakeLists.txt`
- Create: `src/backup/checksum_utils.hpp`
- Create: `src/backup/backup_manifest.hpp`
- Create: `src/backup/backup_service.h`
- Create: `src/backup/backup_service.cpp`
- Create: `test/backup_tests.cpp`

**Interfaces:**
- Consumes: `pcm::database::Database::export_snapshot(const std::string&) const` and `pcm::database::Database::get_application_metadata()` (Task 1; both already existed or were added there).
- Produces:
  - `struct pcm::backup::BackupEntry { std::string path; std::int64_t size_bytes = 0; std::string sha256; };`
  - `struct pcm::backup::BackupManifest { std::int32_t psybackup_format_version = 1; std::int64_t created_at = 0; std::string workspace_uuid; std::int32_t schema_version = 0; std::int32_t backup_format_version = 0; std::string kind; std::vector<BackupEntry> entries; };`
  - `std::string pcm::backup::sha256_file(const std::string &path);` (throws `std::runtime_error` if the file can't be opened)
  - `struct pcm::backup::BackupResult { bool ok = false; std::string error; };`
  - `class pcm::backup::BackupService { public: BackupResult create_backup(const pcm::database::Database &db, const std::string &destination_path); };`
  - Later tasks (3, 4) extend `create_backup`'s signature and add `BackupValidator` — they consume these exact names/types.

#### Step 1: Enable Poco's `zip` feature

In `vcpkg.json`, replace:

```json
    "plog",
    "poco",
```

with:

```json
    "plog",
    {
      "name": "poco",
      "features": ["zip"]
    },
```

- [ ] **Step 1a: Apply the vcpkg.json change above**

- [ ] **Step 1b: Reconfigure and confirm vcpkg installs the Zip component**

Run: `cmake --preset default -B build` (use this repo's normal configure preset/command)
Expected: configure succeeds; vcpkg installs/rebuilds `poco` with the `zip` feature (this rebuilds Poco, so it can take a few minutes the first time).

- [ ] **Step 1c: Commit**

```bash
git add vcpkg.json
git commit -m "build: enable Poco Zip feature for local backups"
```

#### Step 2: Checksum utility, test-first

- [ ] **Step 2a: Create the test file with a failing checksum test**

Create `test/backup_tests.cpp`:

```cpp
#include <Poco/File.h>
#include <Poco/Path.h>
#include <fstream>
#include <gtest/gtest.h>

#include "checksum_utils.hpp"

namespace {

std::string writeTempFile(const std::string &name, const std::string &content) {
  const auto path = Poco::Path(Poco::Path::current()).append(name).toString();
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
  out.close();
  return path;
}

} // namespace

TEST(ChecksumUtilsTest, Sha256FileMatchesKnownVectors) {
  const auto helloPath = writeTempFile("tmp_sha256_hello.txt", "hello world");
  EXPECT_EQ(pcm::backup::sha256_file(helloPath),
           "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
  Poco::File(helloPath).remove();

  const auto emptyPath = writeTempFile("tmp_sha256_empty.txt", "");
  EXPECT_EQ(pcm::backup::sha256_file(emptyPath),
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  Poco::File(emptyPath).remove();
}

TEST(ChecksumUtilsTest, Sha256FileThrowsForMissingFile) {
  const auto missingPath =
      Poco::Path(Poco::Path::current()).append("tmp_does_not_exist.txt").toString();
  EXPECT_THROW(pcm::backup::sha256_file(missingPath), std::runtime_error);
}
```

Note: the two expected hex digests above are the standard SHA-256 test vectors for `"hello world"` and the empty string.

- [ ] **Step 2b: Create the backup module's CMakeLists.txt (compiles nothing yet, but registers the target)**

Create `src/backup/CMakeLists.txt`:

```cmake
set(TARGET_NAME ${PROJECT_NAME}_backup)

find_package(Poco REQUIRED COMPONENTS Foundation Zip)
find_package(reflectcpp CONFIG REQUIRED)

add_library(${TARGET_NAME} STATIC
        backup_manifest.hpp
        checksum_utils.hpp
        backup_service.h
        backup_service.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(${TARGET_NAME} ${PROJECT_NAME}_database)

target_link_libraries(${TARGET_NAME}
  PUBLIC
    ${PROJECT_NAME}_database
    Poco::Foundation
  PRIVATE
    Poco::Zip
    reflectcpp::reflectcpp
)
```

- [ ] **Step 2c: Create `checksum_utils.hpp`**

Create `src/backup/checksum_utils.hpp`:

```cpp
// checksum_utils.hpp
#pragma once

#include <Poco/DigestEngine.h>
#include <Poco/SHA2Engine.h>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>

namespace pcm::backup {

inline std::string sha256_file(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("sha256_file: cannot open file: " + path);
  }

  Poco::SHA2Engine engine(Poco::SHA2Engine::SHA_256);
  std::array<char, 8192> buffer{};
  for (;;) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto n = in.gcount();
    if (n > 0) {
      engine.update(buffer.data(), static_cast<std::size_t>(n));
    }
    if (!in) {
      break;
    }
  }

  return Poco::DigestEngine::digestToHex(engine.digest());
}

} // namespace pcm::backup
```

- [ ] **Step 2d: Create the minimal `backup_manifest.hpp`, `backup_service.h`, `backup_service.cpp` needed to compile (no `create_backup` logic yet — just enough for the target to build)**

Create `src/backup/backup_manifest.hpp`:

```cpp
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
```

Create `src/backup/backup_service.h`:

```cpp
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
```

Create `src/backup/backup_service.cpp`:

```cpp
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
```

- [ ] **Step 2e: Wire the module and test executable into the build**

In `CMakeLists.txt`, replace:

```cmake
add_subdirectory(src/widgets)
add_subdirectory(src/config)
add_subdirectory(src/database)
add_subdirectory(src/client_model)
```

with:

```cmake
add_subdirectory(src/widgets)
add_subdirectory(src/config)
add_subdirectory(src/database)
add_subdirectory(src/backup)
add_subdirectory(src/client_model)
```

In `test/CMakeLists.txt`, append at the end of the file:

```cmake

# Backup tests
find_package(reflectcpp CONFIG REQUIRED)
add_executable(PsyClientManager_backup_tests backup_tests.cpp)
target_link_libraries(PsyClientManager_backup_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_config
    PsyClientManager_database
    PsyClientManager_backup
    Poco::Foundation
    reflectcpp::reflectcpp
)
gtest_discover_tests(PsyClientManager_backup_tests)
```

- [ ] **Step 2f: Run the checksum tests to verify they pass**

Run: `cmake --build build --target PsyClientManager_backup_tests && ctest --test-dir build -R ChecksumUtilsTest --output-on-failure`
Expected: both `ChecksumUtilsTest` cases PASS

- [ ] **Step 2g: Commit**

```bash
git add CMakeLists.txt test/CMakeLists.txt src/backup test/backup_tests.cpp
git commit -m "feat(backup): scaffold src/backup module with SHA-256 checksum utility"
```

#### Step 3: `BackupManifest` JSON round-trip, test-first

- [ ] **Step 3a: Add the failing test**

Append to `test/backup_tests.cpp`:

```cpp
#include "backup_manifest.hpp"
#include <rfl/json.hpp>

TEST(BackupManifestTest, JsonRoundTripPreservesAllFields) {
  pcm::backup::BackupManifest manifest;
  manifest.psybackup_format_version = 1;
  manifest.created_at = 1732550400000;
  manifest.workspace_uuid = "11111111-2222-3333-4444-555555555555";
  manifest.schema_version = 1;
  manifest.backup_format_version = 1;
  manifest.kind = "database";
  manifest.entries.push_back(
      pcm::backup::BackupEntry{"database/Client.parquet", 4821,
                               "0123456789abcdef0123456789abcdef"});

  const auto json = rfl::json::write(manifest, rfl::json::pretty);
  const auto parsed = rfl::json::read<pcm::backup::BackupManifest>(json);
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed.value().psybackup_format_version, 1);
  EXPECT_EQ(parsed.value().created_at, 1732550400000);
  EXPECT_EQ(parsed.value().workspace_uuid,
           "11111111-2222-3333-4444-555555555555");
  EXPECT_EQ(parsed.value().kind, "database");
  ASSERT_EQ(parsed.value().entries.size(), 1u);
  EXPECT_EQ(parsed.value().entries[0].path, "database/Client.parquet");
  EXPECT_EQ(parsed.value().entries[0].size_bytes, 4821);
  EXPECT_EQ(parsed.value().entries[0].sha256,
           "0123456789abcdef0123456789abcdef");
}
```

Add `#include "backup_manifest.hpp"` and `#include <rfl/json.hpp>` to the top of `test/backup_tests.cpp` if not already grouped there (the block above shows them inline for clarity, but keep all includes at the top of the file per the existing style in `database_tests.cpp`).

- [ ] **Step 3b: Run test to verify it passes** (no production code changes needed — `BackupManifest` is a plain aggregate struct, `rfl` reflects it directly, same as `pcm::config::Config`)

Run: `cmake --build build --target PsyClientManager_backup_tests && ctest --test-dir build -R BackupManifestTest --output-on-failure`
Expected: PASS

- [ ] **Step 3c: Commit**

```bash
git add test/backup_tests.cpp
git commit -m "test(backup): verify BackupManifest JSON round-trip via rfl"
```

#### Step 4: `BackupService::create_backup` — database-only backup, test-first

- [ ] **Step 4a: Add the failing test**

Append to `test/backup_tests.cpp`:

```cpp
#include <Poco/Path.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/Zip/Decompress.h>
#include "backup_service.h"
#include "config.h"
#include "database.h"

namespace {

pcm::database::Database makeTestDatabase(const std::string &dirName) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append(dirName)}};
  Poco::File dbDir(conf.db_conf().db_pth);
  if (dbDir.exists()) {
    dbDir.remove(true);
  }
  return pcm::database::Database{conf};
}

pcm::backup::BackupManifest extractManifest(const std::string &backupPath,
                                            const std::string &extractDir) {
  std::ifstream zipIn(backupPath, std::ios::binary);
  Poco::Zip::Decompress decompress(zipIn, Poco::Path(extractDir));
  decompress.decompressAllFiles();
  const auto manifestPath =
      Poco::Path(extractDir).append("manifest.json").toString();
  return rfl::json::load<pcm::backup::BackupManifest>(manifestPath).value();
}

} // namespace

TEST(BackupServiceTest, CreateBackupDatabaseOnlyProducesValidArchive) {
  auto db = makeTestDatabase("tmp_dir_backup_db_only");

  DuckClient client;
  client.name = std::string{"Backup"};
  client.last_name = std::string{"Client"};
  ASSERT_GT(db.add_client(client), 0);

  const auto destPath =
      Poco::Path(Poco::Path::current()).append("tmp_backup_db_only.psybackup")
          .toString();
  Poco::File destFile(destPath);
  if (destFile.exists()) {
    destFile.remove();
  }

  pcm::backup::BackupService service;
  const auto result = service.create_backup(db, destPath);
  ASSERT_TRUE(result.ok) << result.error;
  ASSERT_TRUE(Poco::File(destPath).exists());

  const auto extractDir =
      Poco::Path(Poco::Path::current()).append("tmp_backup_db_only_extract")
          .toString();
  Poco::File extractDirFile(extractDir);
  if (extractDirFile.exists()) {
    extractDirFile.remove(true);
  }

  const auto manifest = extractManifest(destPath, extractDir);
  EXPECT_EQ(manifest.kind, "database");
  ASSERT_FALSE(manifest.entries.empty());

  bool foundClientTable = false;
  for (const auto &entry : manifest.entries) {
    Poco::Path entryPath(extractDir);
    entryPath.append(Poco::Path(entry.path, Poco::Path::PATH_UNIX));
    ASSERT_TRUE(Poco::File(entryPath).exists()) << entry.path;
    EXPECT_EQ(pcm::backup::sha256_file(entryPath.toString()), entry.sha256)
        << entry.path;
    if (entry.path == "database/Client.parquet") {
      foundClientTable = true;
    }
  }
  EXPECT_TRUE(foundClientTable);

  extractDirFile.remove(true);
  destFile.remove();
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_dir_backup_db_only"))
      .remove(true);
}
```

- [ ] **Step 4b: Run test to verify it fails**

Run: `cmake --build build --target PsyClientManager_backup_tests && ctest --test-dir build -R CreateBackupDatabaseOnlyProducesValidArchive --output-on-failure`
Expected: FAIL — `result.ok` is false (`"not implemented"`)

- [ ] **Step 4c: Implement `create_backup` (database-only)**

Replace the full contents of `src/backup/backup_service.cpp` with:

```cpp
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
```

- [ ] **Step 4d: Run test to verify it passes**

Run: `cmake --build build --target PsyClientManager_backup_tests && ctest --test-dir build -R CreateBackupDatabaseOnlyProducesValidArchive --output-on-failure`
Expected: PASS

- [ ] **Step 4e: Run the full backup test binary to make sure nothing regressed**

Run: `ctest --test-dir build -R PsyClientManager_backup_tests --output-on-failure`
Expected: all tests PASS

- [ ] **Step 4f: Commit**

```bash
git add src/backup/backup_service.cpp test/backup_tests.cpp
git commit -m "feat(backup): implement database-only BackupService::create_backup"
```

---

### Task 3: Attachments support

**Files:**
- Modify: `src/backup/backup_service.h`
- Modify: `src/backup/backup_service.cpp`
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Consumes: everything from Task 2 (`BackupManifest`, `BackupEntry`, `sha256_file`, existing `create_backup` internals).
- Produces: widened signature `BackupResult create_backup(const database::Database &db, const std::string &destination_path, const BackupOptions &options = {});` and `struct BackupOptions { std::optional<std::string> attachments_root; };`. The old 2-argument call sites from Task 2's tests keep compiling unchanged (third parameter defaults to `{}`).

- [ ] **Step 1: Write the failing tests**

Append to `test/backup_tests.cpp`:

```cpp
TEST(BackupServiceTest, CreateBackupWithAttachmentsIncludesAttachmentTree) {
  auto db = makeTestDatabase("tmp_dir_backup_with_attachments");

  const auto attachmentsRoot =
      Poco::Path(Poco::Path::current()).append("tmp_attachments_root").toString();
  Poco::File attachmentsRootFile(attachmentsRoot);
  if (attachmentsRootFile.exists()) {
    attachmentsRootFile.remove(true);
  }
  Poco::File(Poco::Path(attachmentsRoot).append("42").append("7")).createDirectories();
  std::ofstream attachmentOut(
      Poco::Path(attachmentsRoot).append("42").append("7").append("note.txt")
          .toString(),
      std::ios::binary | std::ios::trunc);
  attachmentOut << "attachment contents";
  attachmentOut.close();

  const auto destPath =
      Poco::Path(Poco::Path::current()).append("tmp_backup_with_attachments.psybackup")
          .toString();
  Poco::File destFile(destPath);
  if (destFile.exists()) {
    destFile.remove();
  }

  pcm::backup::BackupService service;
  pcm::backup::BackupOptions options;
  options.attachments_root = attachmentsRoot;
  const auto result = service.create_backup(db, destPath, options);
  ASSERT_TRUE(result.ok) << result.error;

  const auto extractDir =
      Poco::Path(Poco::Path::current())
          .append("tmp_backup_with_attachments_extract")
          .toString();
  Poco::File extractDirFile(extractDir);
  if (extractDirFile.exists()) {
    extractDirFile.remove(true);
  }

  const auto manifest = extractManifest(destPath, extractDir);
  EXPECT_EQ(manifest.kind, "database_and_attachments");

  bool foundAttachment = false;
  for (const auto &entry : manifest.entries) {
    if (entry.path == "attachments/42/7/note.txt") {
      foundAttachment = true;
    }
  }
  EXPECT_TRUE(foundAttachment);

  extractDirFile.remove(true);
  destFile.remove();
  attachmentsRootFile.remove(true);
  Poco::File(Poco::Path(Poco::Path::current())
                .append("tmp_dir_backup_with_attachments"))
      .remove(true);
}

TEST(BackupServiceTest, CreateBackupFailsWhenAttachmentsRootMissing) {
  auto db = makeTestDatabase("tmp_dir_backup_missing_attachments");

  const auto destPath =
      Poco::Path(Poco::Path::current())
          .append("tmp_backup_missing_attachments.psybackup")
          .toString();
  Poco::File destFile(destPath);
  if (destFile.exists()) {
    destFile.remove();
  }

  pcm::backup::BackupService service;
  pcm::backup::BackupOptions options;
  options.attachments_root = Poco::Path(Poco::Path::current())
                                 .append("tmp_attachments_does_not_exist")
                                 .toString();
  const auto result = service.create_backup(db, destPath, options);
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.error.empty());
  EXPECT_FALSE(Poco::File(destPath).exists());

  Poco::File(Poco::Path(Poco::Path::current())
                .append("tmp_dir_backup_missing_attachments"))
      .remove(true);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target PsyClientManager_backup_tests`
Expected: FAIL to compile — `create_backup` doesn't accept a third argument, `pcm::backup::BackupOptions` doesn't exist.

- [ ] **Step 3: Widen the `create_backup` signature**

Replace the full contents of `src/backup/backup_service.h` with:

```cpp
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
```

- [ ] **Step 4: Implement attachments handling**

In `src/backup/backup_service.cpp`, change the function signature and insert the attachments block. Replace:

```cpp
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
```

with:

```cpp
BackupResult BackupService::create_backup(const database::Database &db,
                                          const std::string &destination_path,
                                          const BackupOptions &options) {
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
  manifest.created_at =
      static_cast<std::int64_t>(Poco::Timestamp().epochMicroseconds() / 1000);
  const auto metadata = db.get_application_metadata();
  manifest.workspace_uuid = metadata.workspace_uuid;
  manifest.schema_version = metadata.schema_version;
  manifest.backup_format_version = metadata.backup_format_version;
  manifest.kind = kind;
  manifest.entries = collectEntries(scratchDir);
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build --target PsyClientManager_backup_tests && ctest --test-dir build -R BackupServiceTest --output-on-failure`
Expected: all `BackupServiceTest` cases PASS, including the two new ones and the Task 2 database-only test (unaffected by the default argument).

- [ ] **Step 6: Commit**

```bash
git add src/backup/backup_service.h src/backup/backup_service.cpp test/backup_tests.cpp
git commit -m "feat(backup): support including attachments in a backup"
```

---

### Task 4: `BackupValidator`

**Files:**
- Modify: `src/backup/CMakeLists.txt`
- Create: `src/backup/backup_validator.h`
- Create: `src/backup/backup_validator.cpp`
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Consumes: `BackupManifest`, `sha256_file` (Task 2); a `.psybackup` file produced by `BackupService::create_backup` (Tasks 2–3).
- Produces:
  - `struct pcm::backup::ValidationResult { bool ok = false; std::vector<std::string> errors; };`
  - `class pcm::backup::BackupValidator { public: ValidationResult validate(const std::string &backup_path); };`
  - Task 5 consumes `BackupValidator::validate` directly.

- [ ] **Step 1: Write the failing tests**

Add these two includes to the top of `test/backup_tests.cpp`, alongside the existing includes (the `RejectsUnsupportedFormatVersion` test below rebuilds a zip directly):

```cpp
#include <Poco/Zip/Compress.h>
#include <Poco/Zip/ZipCommon.h>
```

Append to `test/backup_tests.cpp`:

```cpp
#include "backup_validator.h"

TEST(BackupValidatorTest, ValidatesACleanBackupAsOk) {
  auto db = makeTestDatabase("tmp_dir_validate_ok");

  DuckClient client;
  client.name = std::string{"Validate"};
  client.last_name = std::string{"Ok"};
  ASSERT_GT(db.add_client(client), 0);

  const auto destPath = Poco::Path(Poco::Path::current())
                            .append("tmp_backup_validate_ok.psybackup")
                            .toString();
  Poco::File destFile(destPath);
  if (destFile.exists()) {
    destFile.remove();
  }

  pcm::backup::BackupService service;
  ASSERT_TRUE(service.create_backup(db, destPath).ok);

  pcm::backup::BackupValidator validator;
  const auto result = validator.validate(destPath);
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(result.errors.empty());

  destFile.remove();
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_dir_validate_ok"))
      .remove(true);
}

TEST(BackupValidatorTest, DetectsCorruptedEntry) {
  auto db = makeTestDatabase("tmp_dir_validate_corrupt");

  DuckClient client;
  client.name = std::string{"Validate"};
  client.last_name = std::string{"Corrupt"};
  ASSERT_GT(db.add_client(client), 0);

  const auto destPath = Poco::Path(Poco::Path::current())
                            .append("tmp_backup_validate_corrupt.psybackup")
                            .toString();
  Poco::File destFile(destPath);
  if (destFile.exists()) {
    destFile.remove();
  }

  pcm::backup::BackupService service;
  ASSERT_TRUE(service.create_backup(db, destPath).ok);

  {
    std::fstream f(destPath, std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(f);
    f.seekg(0, std::ios::end);
    const std::streamoff size = f.tellg();
    ASSERT_GT(size, 100);
    const std::streamoff mid = size / 2;
    f.seekg(mid, std::ios::beg);
    char original = 0;
    f.read(&original, 1);
    const char corrupted = static_cast<char>(~original);
    f.seekp(mid, std::ios::beg);
    f.write(&corrupted, 1);
  }

  pcm::backup::BackupValidator validator;
  const auto result = validator.validate(destPath);
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errors.empty());

  destFile.remove();
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_dir_validate_corrupt"))
      .remove(true);
}

TEST(BackupValidatorTest, RejectsUnsupportedFormatVersion) {
  auto db = makeTestDatabase("tmp_dir_validate_version");

  const auto destPath = Poco::Path(Poco::Path::current())
                            .append("tmp_backup_validate_version.psybackup")
                            .toString();
  Poco::File destFile(destPath);
  if (destFile.exists()) {
    destFile.remove();
  }

  pcm::backup::BackupService service;
  ASSERT_TRUE(service.create_backup(db, destPath).ok);

  const auto extractDir = Poco::Path(Poco::Path::current())
                              .append("tmp_backup_validate_version_extract")
                              .toString();
  Poco::File extractDirFile(extractDir);
  if (extractDirFile.exists()) {
    extractDirFile.remove(true);
  }
  auto manifest = extractManifest(destPath, extractDir);
  manifest.psybackup_format_version = 999;
  ASSERT_TRUE(rfl::json::save(
      Poco::Path(extractDir).append("manifest.json").toString(), manifest,
      rfl::json::pretty));

  const auto tamperedPath = Poco::Path(Poco::Path::current())
                                .append("tmp_backup_validate_version_tampered.psybackup")
                                .toString();
  Poco::File tamperedFile(tamperedPath);
  if (tamperedFile.exists()) {
    tamperedFile.remove();
  }
  {
    std::ofstream zipOut(tamperedPath, std::ios::binary | std::ios::trunc);
    Poco::Zip::Compress compress(zipOut, true);
    compress.addRecursive(Poco::Path(extractDir),
                          Poco::Zip::ZipCommon::CL_MAXIMUM, true);
    compress.close();
  }

  pcm::backup::BackupValidator validator;
  const auto result = validator.validate(tamperedPath);
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errors.empty());

  extractDirFile.remove(true);
  destFile.remove();
  tamperedFile.remove();
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_dir_validate_version"))
      .remove(true);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target PsyClientManager_backup_tests`
Expected: FAIL to compile — `backup_validator.h` does not exist yet.

- [ ] **Step 3: Create `backup_validator.h`**

Create `src/backup/backup_validator.h`:

```cpp
// backup_validator.h
#pragma once

#include <string>
#include <vector>

namespace pcm::backup {

struct ValidationResult {
  bool ok = false;
  std::vector<std::string> errors;
};

class BackupValidator {
public:
  ValidationResult validate(const std::string &backup_path);
};

} // namespace pcm::backup
```

- [ ] **Step 4: Create `backup_validator.cpp`**

Create `src/backup/backup_validator.cpp`:

```cpp
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

} // namespace

ValidationResult BackupValidator::validate(const std::string &backup_path) {
  ValidationResult result;

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

  try {
    Poco::Zip::Decompress decompress(zipIn, Poco::Path(extractDir));
    decompress.decompressAllFiles();
  } catch (const Poco::Exception &ex) {
    result.errors.push_back("failed to open backup as a zip archive: " +
                            ex.displayText());
    return result;
  }

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
}

} // namespace pcm::backup
```

- [ ] **Step 5: Register the new sources**

In `src/backup/CMakeLists.txt`, replace:

```cmake
add_library(${TARGET_NAME} STATIC
        backup_manifest.hpp
        checksum_utils.hpp
        backup_service.h
        backup_service.cpp
)
```

with:

```cmake
add_library(${TARGET_NAME} STATIC
        backup_manifest.hpp
        checksum_utils.hpp
        backup_service.h
        backup_service.cpp
        backup_validator.h
        backup_validator.cpp
)
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cmake --build build --target PsyClientManager_backup_tests && ctest --test-dir build -R BackupValidatorTest --output-on-failure`
Expected: all three `BackupValidatorTest` cases PASS

- [ ] **Step 7: Commit**

```bash
git add src/backup/CMakeLists.txt src/backup/backup_validator.h src/backup/backup_validator.cpp test/backup_tests.cpp
git commit -m "feat(backup): add BackupValidator for manifest/checksum verification"
```

---

### Task 5: Atomicity — failed backup never touches an existing destination

**Files:**
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Consumes: `BackupService::create_backup` (Task 3's 3-argument form), `BackupOptions::attachments_root` failure path (Task 3, Step 4 — a missing `attachments_root` returns `{false, ...}` before the temp zip or rename happens), `BackupValidator::validate` (Task 4).
- Produces: nothing new — this task only adds a regression test for the atomicity guarantee already implemented in Task 2/3 (destination is only touched via the final `renameTo`, and the missing-`attachments_root` check happens before any scratch/zip work is finalized).

- [ ] **Step 1: Write the test**

Append to `test/backup_tests.cpp`:

```cpp
TEST(BackupServiceTest, FailedBackupLeavesExistingDestinationUntouched) {
  auto db = makeTestDatabase("tmp_dir_atomicity");

  DuckClient client;
  client.name = std::string{"Atomic"};
  client.last_name = std::string{"Ity"};
  ASSERT_GT(db.add_client(client), 0);

  const auto destPath = Poco::Path(Poco::Path::current())
                            .append("tmp_backup_atomicity.psybackup")
                            .toString();
  Poco::File destFile(destPath);
  if (destFile.exists()) {
    destFile.remove();
  }

  pcm::backup::BackupService service;
  ASSERT_TRUE(service.create_backup(db, destPath).ok);

  std::ifstream before(destPath, std::ios::binary);
  const std::string beforeContents((std::istreambuf_iterator<char>(before)),
                                   std::istreambuf_iterator<char>());
  before.close();

  pcm::backup::BackupValidator validator;
  ASSERT_TRUE(validator.validate(destPath).ok);

  pcm::backup::BackupOptions options;
  options.attachments_root = Poco::Path(Poco::Path::current())
                                 .append("tmp_atomicity_missing_attachments")
                                 .toString();
  const auto failedResult = service.create_backup(db, destPath, options);
  EXPECT_FALSE(failedResult.ok);

  std::ifstream after(destPath, std::ios::binary);
  const std::string afterContents((std::istreambuf_iterator<char>(after)),
                                  std::istreambuf_iterator<char>());
  after.close();

  EXPECT_EQ(beforeContents, afterContents);
  EXPECT_TRUE(validator.validate(destPath).ok);

  const auto parentDir = Poco::Path(Poco::Path::current());
  Poco::DirectoryIterator dirIt(parentDir);
  const Poco::DirectoryIterator dirEnd;
  for (; dirIt != dirEnd; ++dirIt) {
    EXPECT_EQ(dirIt.name().find("tmp_backup_atomicity.psybackup.partial-"),
             std::string::npos)
        << "leftover partial file: " << dirIt.name();
  }

  destFile.remove();
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_dir_atomicity"))
      .remove(true);
}
```

Add `#include <Poco/DirectoryIterator.h>` to the top of `test/backup_tests.cpp`.

- [ ] **Step 2: Run the test**

Run: `cmake --build build --target PsyClientManager_backup_tests && ctest --test-dir build -R FailedBackupLeavesExistingDestinationUntouched --output-on-failure`
Expected: PASS — no production code changes needed; this confirms the guarantee already built into Tasks 2–3 (the `attachments_root` existence check runs before any scratch directory work is finalized or renamed into place).

- [ ] **Step 3: Run the entire test suite to confirm no regressions**

Run: `ctest --test-dir build --output-on-failure`
Expected: all tests PASS (config, database, schedule_conflict, backup)

- [ ] **Step 4: Commit**

```bash
git add test/backup_tests.cpp
git commit -m "test(backup): verify a failed backup leaves an existing destination untouched"
```

---

### Task 6: Docs, version, and changelog

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `CHANGELOG.md`
- Modify: `README.md`
- Modify: `docs/asciidoc/05-modules.adoc`
- Modify: `docs/asciidoc/06-database-and-data-model.adoc`
- Modify: `docs/asciidoc/07-testing.adoc`

**Interfaces:**
- Consumes: nothing (documentation only).
- Produces: nothing consumed by later tasks — this is the last task.

- [ ] **Step 1: Bump the project version**

In `CMakeLists.txt`, replace:

```cmake
project(PsyClientManager VERSION 0.1.8 LANGUAGES CXX)
```

with:

```cmake
project(PsyClientManager VERSION 0.1.9 LANGUAGES CXX)
```

- [ ] **Step 2: Add a CHANGELOG entry**

In `CHANGELOG.md`, insert this new section right after the `# Changelog` header and its intro line, before the existing `## [0.1.8] - 2026-07-25` entry:

```markdown
## [0.1.9] - 2026-07-25

### Added

- Local `.psybackup` backup format: `BackupService` writes a consistent DuckDB
  snapshot (via `EXPORT DATABASE ... FORMAT PARQUET`), optional attachments,
  and a SHA-256 checksummed manifest into a zip archive, finalized atomically.
- `BackupValidator` to verify a `.psybackup` file's manifest and checksums.
- Database and attachment backup coverage in the test suite.

```

- [ ] **Step 3: Add a README feature line**

In `README.md`, replace:

```markdown
- Persistent workspace and schema metadata for future backup/restore workflows
```

with:

```markdown
- Persistent workspace and schema metadata for future backup/restore workflows
- Local `.psybackup` backups of the database (and optionally attachments), with checksum validation
```

- [ ] **Step 4: Document the new module**

In `docs/asciidoc/05-modules.adoc`, insert this new section right after the `=== src/database` section (after its last bullet, `db_utils.hpp`, and before `=== src/client_model`):

```asciidoc
=== `src/backup`

- `BackupService`
: builds a `.psybackup` zip (consistent DuckDB export via
  `Database::export_snapshot`, optional attachments, SHA-256 checksummed
  `manifest.json`), finalized with a same-directory temp file + atomic
  rename.
- `BackupValidator`
: re-verifies a `.psybackup` file's manifest and per-entry checksums.
- `backup_manifest.hpp`
: `BackupManifest`/`BackupEntry` DTOs (rfl JSON).
- `checksum_utils.hpp`
: `sha256_file` via `Poco::SHA2Engine`.

```

- [ ] **Step 5: Cross-reference from the database docs**

In `docs/asciidoc/06-database-and-data-model.adoc`, replace:

```asciidoc
- `backup_format_version` reserves a version for future `.psybackup` files;
```

with:

```asciidoc
- `backup_format_version` reserves a version for `.psybackup` files, written
  by `BackupService` in `src/backup` (see <<Modules>>);
```

- [ ] **Step 6: Update the testing doc**

In `docs/asciidoc/07-testing.adoc`, replace:

```asciidoc
- `database_tests.cpp`
: DB initialization test with temporary directory.

Framework: GoogleTest (`GTest`).
```

with:

```asciidoc
- `database_tests.cpp`
: DB initialization test with temporary directory.
- `backup_tests.cpp`
: checksum utility, manifest round-trip, database/attachment backup
  creation, validator, and atomicity coverage.

Framework: GoogleTest (`GTest`).
```

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt CHANGELOG.md README.md docs/asciidoc/05-modules.adoc docs/asciidoc/06-database-and-data-model.adoc docs/asciidoc/07-testing.adoc
git commit -m "docs: document local backup service and bump version to 0.1.9"
```
