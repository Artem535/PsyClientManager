#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/Zip/Decompress.h>
#include <fstream>
#include <gtest/gtest.h>
#include <rfl/json.hpp>

#include "backup_manifest.hpp"
#include "backup_service.h"
#include "checksum_utils.hpp"
#include "config.h"
#include "database.h"

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
    if (entry.path == "database/client.parquet") {
      foundClientTable = true;
    }
  }
  EXPECT_TRUE(foundClientTable);

  extractDirFile.remove(true);
  destFile.remove();
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_dir_backup_db_only"))
      .remove(true);
}

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
