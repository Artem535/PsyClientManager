#pragma once

#include <string>

namespace pcm::backup {

struct RotationResult {
  bool ok = false;
  std::string error;
  int removed_count = 0;
};

class BackupRotationService {
public:
  RotationResult prune(const std::string &directory,
                       const std::string &filename_prefix,
                       int keep_count);
};

} // namespace pcm::backup
