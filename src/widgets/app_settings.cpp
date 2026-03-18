#include "app_settings.h"

#include <QColor>
#include <QSettings>

namespace {
constexpr auto kConfirmEventDeletionKey = "event/confirmDeletion";
constexpr auto kPreventEventOverlapsKey = "event/preventOverlaps";
constexpr auto kShowStatusBarMessagesKey = "ui/showStatusBarMessages";
constexpr auto kLanguageCodeKey = "ui/language";
constexpr auto kWorkEventColorKey = "timeline/workEventColor";
constexpr auto kPersonalEventColorKey = "timeline/personalEventColor";

QColor defaultWorkEventColor() {
  return QColor(173, 216, 230);
}

QColor defaultPersonalEventColor() {
  return QColor(255, 182, 193);
}
} // namespace

namespace pcm::app_settings {

bool confirmEventDeletion() {
  QSettings settings;
  return settings.value(kConfirmEventDeletionKey, true).toBool();
}

void setConfirmEventDeletion(const bool enabled) {
  QSettings settings;
  settings.setValue(kConfirmEventDeletionKey, enabled);
}

bool preventEventOverlaps() {
  QSettings settings;
  return settings.value(kPreventEventOverlapsKey, true).toBool();
}

void setPreventEventOverlaps(const bool enabled) {
  QSettings settings;
  settings.setValue(kPreventEventOverlapsKey, enabled);
}

bool showStatusBarMessages() {
  QSettings settings;
  return settings.value(kShowStatusBarMessagesKey, true).toBool();
}

void setShowStatusBarMessages(const bool enabled) {
  QSettings settings;
  settings.setValue(kShowStatusBarMessagesKey, enabled);
}

QString languageCode() {
  QSettings settings;
  return settings.value(kLanguageCodeKey, QStringLiteral("system")).toString();
}

void setLanguageCode(const QString &languageCode) {
  QSettings settings;
  settings.setValue(kLanguageCodeKey, languageCode);
}

QColor workEventColor() {
  QSettings settings;
  return settings.value(kWorkEventColorKey, defaultWorkEventColor()).value<QColor>();
}

void setWorkEventColor(const QColor &color) {
  QSettings settings;
  settings.setValue(kWorkEventColorKey, color);
}

QColor personalEventColor() {
  QSettings settings;
  return settings.value(kPersonalEventColorKey, defaultPersonalEventColor()).value<QColor>();
}

void setPersonalEventColor(const QColor &color) {
  QSettings settings;
  settings.setValue(kPersonalEventColorKey, color);
}

} // namespace pcm::app_settings
