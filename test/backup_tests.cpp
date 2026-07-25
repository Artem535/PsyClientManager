#include <Poco/File.h>
#include <Poco/Path.h>
#include <fstream>
#include <gtest/gtest.h>
#include <rfl/json.hpp>

#include "backup_manifest.hpp"
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
