#pragma once

#include <cstdint>

namespace pcm::backup {

bool isAutoBackupDue(bool enabled, std::int64_t lastRunAtMs, int intervalDays,
                     std::int64_t nowMs);

} // namespace pcm::backup
