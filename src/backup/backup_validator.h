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
