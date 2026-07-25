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
