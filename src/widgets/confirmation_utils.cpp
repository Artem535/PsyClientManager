#include "confirmation_utils.h"

#include "app_settings.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QLocale>
#include <QTimeZone>

namespace pcm::confirmation {

QString buildConfirmationRequestText(const QString &clientName,
                                     const qint64 startDateTimeUtcMs) {
  const auto startDateTime =
      QDateTime::fromMSecsSinceEpoch(startDateTimeUtcMs, QTimeZone::UTC).toLocalTime();
  const auto date = QLocale().toString(startDateTime.date(), QLocale::ShortFormat);
  const auto time = QLocale().toString(startDateTime.time(), QLocale::ShortFormat);

  auto text = pcm::app_settings::confirmationRequestTemplate();
  text.replace(QStringLiteral("{client_name}"), clientName.trimmed());
  text.replace(QStringLiteral("{date}"), date);
  text.replace(QStringLiteral("{time}"), time);
  return text;
}

void copyConfirmationRequest(const QString &clientName,
                             const qint64 startDateTimeUtcMs) {
  if (auto *clipboard = QApplication::clipboard()) {
    clipboard->setText(buildConfirmationRequestText(clientName, startDateTimeUtcMs));
  }
}

} // namespace pcm::confirmation
