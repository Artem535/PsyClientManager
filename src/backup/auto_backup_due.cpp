#include "auto_backup_due.h"

namespace pcm::backup {

bool isAutoBackupDue(const bool enabled, const std::int64_t lastRunAtMs,
                     const int intervalDays, const std::int64_t nowMs) {
  if (!enabled) {
    return false;
  }
  if (lastRunAtMs <= 0) {
    return true;
  }
  const std::int64_t intervalMs =
      static_cast<std::int64_t>(intervalDays) * 24 * 60 * 60 * 1000;
  return (nowMs - lastRunAtMs) >= intervalMs;
}

} // namespace pcm::backup
