#include "notification_text.h"

#include <QCoreApplication>
#include <QLocale>

namespace pcm {

QString notificationTitle(const NotificationPrivacyMode mode) {
  switch (mode) {
  case NotificationPrivacyMode::Hidden:
    return QCoreApplication::translate("Notifications", "PsyClientManager");
  case NotificationPrivacyMode::Full:
  case NotificationPrivacyMode::Minimal:
  default:
    return QCoreApplication::translate("Notifications", "Upcoming session");
  }
}

QString notificationBody(const NotificationPrivacyMode mode, const NotificationEventInfo &info) {
  switch (mode) {
  case NotificationPrivacyMode::Hidden:
    return QCoreApplication::translate("Notifications", "Scheduled session");

  case NotificationPrivacyMode::Minimal:
    return QLocale().toString(info.startTime, QStringLiteral("HH:mm"));

  case NotificationPrivacyMode::Full:
  default: {
    const QString timeText = QLocale().toString(info.startTime, QStringLiteral("dd.MM.yyyy HH:mm"));
    const QString title = info.title.isEmpty()
                             ? QCoreApplication::translate("Notifications", "Session")
                             : info.title;
    QString body =
        QCoreApplication::translate("Notifications", "%1 at %2").arg(title, timeText);
    if (info.isWorkEvent && !info.clientName.trimmed().isEmpty()) {
      body += QStringLiteral("\n") +
              QCoreApplication::translate("Notifications", "Client: %1")
                  .arg(info.clientName.trimmed());
    }
    return body;
  }
  }
}

} // namespace pcm
