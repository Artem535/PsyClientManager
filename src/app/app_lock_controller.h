#pragma once

#include <QtTypes>

namespace pcm {

class AppLockController {
 public:
  explicit AppLockController(int timeout_minutes);

  void setTimeoutMinutes(int timeout_minutes);
  void recordActivity(qint64 now_ms);
  void lockNow();
  void unlock(qint64 now_ms);
  bool shouldLock(qint64 now_ms) const;
  bool isLocked() const;

 private:
  qint64 mLastActivityMs = 0;
  qint64 mTimeoutMs = 0;
  bool mLocked = false;
};

} // namespace pcm
