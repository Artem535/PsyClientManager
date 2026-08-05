#include "app_lock_controller.h"

#include <algorithm>

namespace pcm {

AppLockController::AppLockController(const int timeout_minutes)
    : mTimeoutMs(static_cast<qint64>(std::max(0, timeout_minutes)) * 60 * 1000) {}

void AppLockController::recordActivity(const qint64 now_ms) {
  mLastActivityMs = now_ms;
  mManualLockRequested = false;
}

void AppLockController::lockNow() {
  mManualLockRequested = true;
}

bool AppLockController::shouldLock(const qint64 now_ms) const {
  return mManualLockRequested ||
         (mTimeoutMs > 0 && now_ms - mLastActivityMs >= mTimeoutMs);
}

} // namespace pcm
