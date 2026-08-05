#include "app_lock_controller.h"

#include <algorithm>

namespace pcm {

AppLockController::AppLockController(const int timeout_minutes)
{
  setTimeoutMinutes(timeout_minutes);
}

void AppLockController::setTimeoutMinutes(const int timeout_minutes) {
  mTimeoutMs = static_cast<qint64>(std::max(0, timeout_minutes)) * 60 * 1000;
}

void AppLockController::recordActivity(const qint64 now_ms) {
  if (mLocked) {
    return;
  }
  mLastActivityMs = now_ms;
}

void AppLockController::lockNow() {
  mLocked = true;
}

void AppLockController::unlock(const qint64 now_ms) {
  mLocked = false;
  mLastActivityMs = now_ms;
}

bool AppLockController::shouldLock(const qint64 now_ms) const {
  return !mLocked &&
         (mTimeoutMs > 0 && now_ms - mLastActivityMs >= mTimeoutMs);
}

bool AppLockController::isLocked() const { return mLocked; }

} // namespace pcm
