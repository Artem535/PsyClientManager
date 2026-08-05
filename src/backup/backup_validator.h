// backup_validator.h
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace pcm::backup {

struct ValidationResult {
  bool ok = false;
  std::vector<std::string> errors;
};

namespace detail {

class NormalizedBackupFile {
public:
  explicit NormalizedBackupFile(std::string zip_path, std::string scratch_path = {});
  ~NormalizedBackupFile();

  NormalizedBackupFile(const NormalizedBackupFile &) = delete;
  NormalizedBackupFile &operator=(const NormalizedBackupFile &) = delete;
  NormalizedBackupFile(NormalizedBackupFile &&other) noexcept;
  NormalizedBackupFile &operator=(NormalizedBackupFile &&other) noexcept;

  const std::string &zip_path() const;

private:
  std::string mZipPath;
  std::string mScratchPath;
};

std::optional<NormalizedBackupFile>
normalize_backup_file(const std::string &backup_path,
                      const std::optional<std::string> &recovery_password,
                      std::string *error);

} // namespace detail

class BackupValidator {
public:
  ValidationResult validate(
      const std::string &backup_path,
      const std::optional<std::string> &recovery_password = std::nullopt);
};

} // namespace pcm::backup
