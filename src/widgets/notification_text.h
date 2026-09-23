#pragma once

#include <QDateTime>
#include <QString>

namespace pcm {

// Controls how much a session-reminder desktop notification reveals.
// Values are persisted (see app_settings::notificationPrivacyMode), so
// existing numeric values must not be reassigned.
enum class NotificationPrivacyMode {
  Full = 0,   // Session title, time, and client name.
  Hidden = 1, // Generic "Scheduled session" text only.
  Minimal = 2 // Time only, no title or client name.
};

struct NotificationEventInfo {
  QString title;
  QDateTime startTime;
  bool isWorkEvent = false;
  QString clientName;
};

QString notificationTitle(NotificationPrivacyMode mode);

// clientName in `info` is only surfaced in Full mode, and only when
// info.isWorkEvent is true.
QString notificationBody(NotificationPrivacyMode mode, const NotificationEventInfo &info);

} // namespace pcm
