#include <Poco/DirectoryIterator.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/Zip/Compress.h>
#include <Poco/Zip/Decompress.h>
#include <Poco/Zip/ZipCommon.h>
#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <rfl/json.hpp>

#include "backup_manifest.hpp"
#include "backup_service.h"
#include "backup_validator.h"
#include "checksum_utils.hpp"
#include "config.h"
#include "database.h"
#include "restore_service.h"

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

  const auto meta = db.get_application_metadata();
  EXPECT_EQ(manifest.workspace_uuid, meta.workspace_uuid);
  EXPECT_EQ(manifest.schema_version, meta.schema_version);
  EXPECT_EQ(manifest.backup_format_version, meta.backup_format_version);

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
    Poco::Path entryPath(extractDir);
    entryPath.append(Poco::Path(entry.path, Poco::Path::PATH_UNIX));
    ASSERT_TRUE(Poco::File(entryPath).exists()) << entry.path;
    EXPECT_EQ(pcm::backup::sha256_file(entryPath.toString()), entry.sha256)
        << entry.path;
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

TEST(BackupServiceTest, FailedRenameLeavesNoPartialFile) {
  auto db = makeTestDatabase("tmp_dir_failed_rename");

  const auto destPath = Poco::Path(Poco::Path::current())
                            .append("tmp_backup_failed_rename.psybackup")
                            .toString();
  Poco::File destBlocker(destPath);
  if (destBlocker.exists()) {
    destBlocker.remove(true);
  }
  destBlocker.createDirectories();

  pcm::backup::BackupService service;
  const auto result = service.create_backup(db, destPath);
  EXPECT_FALSE(result.ok);

  Poco::File destAfter(destPath);
  EXPECT_TRUE(destAfter.exists());
  EXPECT_TRUE(destAfter.isDirectory());

  const auto parentDir = Poco::Path(Poco::Path::current());
  Poco::DirectoryIterator dirIt(parentDir);
  const Poco::DirectoryIterator dirEnd;
  for (; dirIt != dirEnd; ++dirIt) {
    EXPECT_EQ(dirIt.name().find("tmp_backup_failed_rename.psybackup.partial-"),
             std::string::npos)
        << "leftover partial file: " << dirIt.name();
  }

  destBlocker.remove(true);
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_dir_failed_rename"))
      .remove(true);
}

TEST(RestoreServiceTest, RestoresDatabaseSnapshotIntoNewDirectory) {
  auto sourceDb = makeTestDatabase("tmp_restore_source");
  DuckClient client;
  client.name = std::string{"Restored"};
  client.last_name = std::string{"Client"};
  ASSERT_GT(sourceDb.add_client(client), 0);

  const auto backupPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_database.psybackup")
                              .toString();
  if (Poco::File(backupPath).exists()) {
    Poco::File(backupPath).remove();
  }
  ASSERT_TRUE(pcm::backup::BackupService{}.create_backup(sourceDb, backupPath).ok);

  const auto targetPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_target")
                              .toString();
  if (Poco::File(targetPath).exists()) {
    Poco::File(targetPath).remove(true);
  }

  const auto restoreResult =
      pcm::backup::RestoreService{}.restore_backup(backupPath, targetPath);
  ASSERT_TRUE(restoreResult.ok) << restoreResult.error;
  EXPECT_TRUE(restoreResult.protective_database_path.empty());

  pcm::config::Config targetConfig{
      .db_conf = pcm::config::DatabaseConfig{.db_pth = Poco::Path(targetPath)}};
  pcm::database::Database restoredDb{targetConfig};
  const auto clients = restoredDb.get_clients();
  ASSERT_TRUE(std::any_of(
      clients.begin(), clients.end(), [](const auto &storedClient) {
        return storedClient && storedClient->name.value_or("") == "Restored";
      }));

  Poco::File(backupPath).remove();
  Poco::File(targetPath).remove(true);
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_restore_source"))
      .remove(true);
}

TEST(RestoreServiceTest, PreservesCurrentDatabaseBeforeReplacement) {
  auto sourceDb = makeTestDatabase("tmp_restore_replace_source");
  DuckClient sourceClient;
  sourceClient.name = std::string{"New"};
  ASSERT_GT(sourceDb.add_client(sourceClient), 0);

  const auto backupPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_replace.psybackup")
                              .toString();
  if (Poco::File(backupPath).exists()) {
    Poco::File(backupPath).remove();
  }
  ASSERT_TRUE(pcm::backup::BackupService{}.create_backup(sourceDb, backupPath).ok);

  const auto targetPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_replace_target")
                              .toString();
  {
    auto targetDb = makeTestDatabase("tmp_restore_replace_target");
    DuckClient oldClient;
    oldClient.name = std::string{"Old"};
    ASSERT_GT(targetDb.add_client(oldClient), 0);
  }

  const auto restoreResult =
      pcm::backup::RestoreService{}.restore_backup(backupPath, targetPath);
  ASSERT_TRUE(restoreResult.ok) << restoreResult.error;
  ASSERT_FALSE(restoreResult.protective_database_path.empty());
  EXPECT_TRUE(Poco::File(restoreResult.protective_database_path).exists());

  pcm::config::Config restoredConfig{
      .db_conf = pcm::config::DatabaseConfig{.db_pth = Poco::Path(targetPath)}};
  pcm::database::Database restoredDb{restoredConfig};
  const auto restoredClients = restoredDb.get_clients();
  ASSERT_TRUE(std::any_of(
      restoredClients.begin(), restoredClients.end(), [](const auto &client) {
        return client && client->name.value_or("") == "New";
      }));

  pcm::config::Config protectiveConfig{.db_conf = pcm::config::DatabaseConfig{
      .db_pth = Poco::Path(restoreResult.protective_database_path)}};
  pcm::database::Database protectiveDb{protectiveConfig};
  const auto oldClients = protectiveDb.get_clients();
  EXPECT_TRUE(std::any_of(
      oldClients.begin(), oldClients.end(), [](const auto &client) {
        return client && client->name.value_or("") == "Old";
      }));

  Poco::File(backupPath).remove();
  Poco::File(targetPath).remove(true);
  Poco::File(restoreResult.protective_database_path).remove(true);
  Poco::File(Poco::Path(Poco::Path::current())
                 .append("tmp_restore_replace_source"))
      .remove(true);
}

TEST(RestoreServiceTest, RestoresAttachmentsAlongsideDatabase) {
  auto sourceDb = makeTestDatabase("tmp_restore_attachment_source");
  const auto sourceAttachments = Poco::Path(Poco::Path::current())
                                     .append("tmp_restore_attachment_source_files")
                                     .toString();
  if (Poco::File(sourceAttachments).exists()) {
    Poco::File(sourceAttachments).remove(true);
  }
  Poco::File(sourceAttachments).createDirectories();
  const auto sourceFile = Poco::Path(sourceAttachments).append("note.txt").toString();
  std::ofstream(sourceFile) << "protected note";

  const auto backupPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_attachment.psybackup")
                              .toString();
  if (Poco::File(backupPath).exists()) {
    Poco::File(backupPath).remove();
  }
  pcm::backup::BackupOptions backupOptions;
  backupOptions.attachments_root = sourceAttachments;
  ASSERT_TRUE(pcm::backup::BackupService{}
                  .create_backup(sourceDb, backupPath, backupOptions)
                  .ok);

  const auto targetPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_attachment_target")
                              .toString();
  const auto targetAttachments = Poco::Path(Poco::Path::current())
                                     .append("tmp_restore_attachment_target_files")
                                     .toString();
  if (Poco::File(targetPath).exists()) {
    Poco::File(targetPath).remove(true);
  }
  if (Poco::File(targetAttachments).exists()) {
    Poco::File(targetAttachments).remove(true);
  }

  pcm::backup::RestoreOptions restoreOptions;
  restoreOptions.attachments_root = targetAttachments;
  const auto restoreResult = pcm::backup::RestoreService{}.restore_backup(
      backupPath, targetPath, restoreOptions);
  ASSERT_TRUE(restoreResult.ok) << restoreResult.error;

  const auto restoredFile =
      Poco::Path(targetAttachments).append("note.txt").toString();
  ASSERT_TRUE(Poco::File(restoredFile).exists());
  std::ifstream restoredInput(restoredFile);
  const std::string contents((std::istreambuf_iterator<char>(restoredInput)),
                             std::istreambuf_iterator<char>());
  EXPECT_EQ(contents, "protected note");

  Poco::File(backupPath).remove();
  Poco::File(targetPath).remove(true);
  Poco::File(targetAttachments).remove(true);
  Poco::File(sourceAttachments).remove(true);
  Poco::File(Poco::Path(Poco::Path::current())
                 .append("tmp_restore_attachment_source"))
      .remove(true);
}

TEST(RestoreServiceTest, RejectsInvalidBackupWithoutChangingTarget) {
  const auto targetPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_invalid_target")
                              .toString();
  {
    auto targetDb = makeTestDatabase("tmp_restore_invalid_target");
    DuckClient oldClient;
    oldClient.name = std::string{"Safe"};
    ASSERT_GT(targetDb.add_client(oldClient), 0);
  }

  const auto invalidPath = Poco::Path(Poco::Path::current())
                               .append("tmp_invalid_restore.psybackup")
                               .toString();
  if (Poco::File(invalidPath).exists()) {
    Poco::File(invalidPath).remove();
  }
  std::ofstream invalid(invalidPath, std::ios::binary | std::ios::trunc);
  invalid << "not a backup";
  invalid.close();

  const auto restoreResult =
      pcm::backup::RestoreService{}.restore_backup(invalidPath, targetPath);
  EXPECT_FALSE(restoreResult.ok);
  EXPECT_TRUE(Poco::File(targetPath).exists());

  Poco::File(invalidPath).remove();
  Poco::File(targetPath).remove(true);
}
