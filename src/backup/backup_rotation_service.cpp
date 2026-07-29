#include "backup_rotation_service.h"

#include <Poco/DirectoryIterator.h>
#include <Poco/Exception.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace pcm::backup {

RotationResult BackupRotationService::prune(const std::string &directory,
                                            const std::string &filename_prefix,
                                            const int keep_count) {
  try {
    const Poco::File dir(directory);
    if (!dir.exists() || !dir.isDirectory()) {
      return {true, {}, 0};
    }

    std::vector<std::string> matching;
    Poco::DirectoryIterator it(dir);
    const Poco::DirectoryIterator end;
    for (; it != end; ++it) {
      if (!it->isFile()) {
        continue;
      }
      const auto name = Poco::Path(it->path()).getFileName();
      if (name.rfind(filename_prefix, 0) == 0) {
        matching.push_back(it->path());
      }
    }

    // Auto-backup filenames embed a zero-padded timestamp
    // (PsyClientManager-auto-yyyyMMdd-HHmmss.psybackup), so descending
    // lexicographic order is descending chronological order.
    std::sort(matching.begin(), matching.end(), std::greater<>());

    int removed = 0;
    const auto keepFrom = static_cast<std::size_t>(std::max(keep_count, 0));
    for (std::size_t i = keepFrom; i < matching.size(); ++i) {
      Poco::File(matching[i]).remove();
      ++removed;
    }
    return {true, {}, removed};
  } catch (const Poco::Exception &ex) {
    return {false, "failed to prune backups: " + ex.displayText(), 0};
  } catch (const std::exception &ex) {
    return {false, std::string("failed to prune backups: ") + ex.what(), 0};
  }
}

} // namespace pcm::backup
