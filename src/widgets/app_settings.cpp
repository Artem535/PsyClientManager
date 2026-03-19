#include "app_settings.h"

#include <QColor>
#include <QSettings>
#include <QTime>

namespace {
constexpr auto kConfirmEventDeletionKey = "event/confirmDeletion";
constexpr auto kPreventEventOverlapsKey = "event/preventOverlaps";
constexpr auto kShowStatusBarMessagesKey = "ui/showStatusBarMessages";
constexpr auto kLanguageCodeKey = "ui/language";
constexpr auto kWorkEventColorKey = "timeline/workEventColor";
constexpr auto kPersonalEventColorKey = "timeline/personalEventColor";
constexpr auto kDefaultWorkEventCostKey = "event/defaultWorkEventCost";
constexpr auto kWorkDayStartKey = "event/workDayStart";
constexpr auto kWorkDayEndKey = "event/workDayEnd";
constexpr auto kDefaultSessionDurationMinutesKey = "event/defaultSessionDurationMinutes";

QColor defaultWorkEventColor() {
  return QColor(173, 216, 230);
}

QColor defaultPersonalEventColor() {
  return QColor(255, 182, 193);
}

double defaultWorkEventCostValue() {
  return 2500.0;
}

QTime defaultWorkDayStartValue() {
  return QTime(9, 0);
}

QTime defaultWorkDayEndValue() {
  return QTime(18, 0);
}

int defaultSessionDurationMinutesValue() {
  return 60;
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

double defaultWorkEventCost() {
  QSettings settings;
  return settings.value(kDefaultWorkEventCostKey, defaultWorkEventCostValue())
      .toDouble();
}

void setDefaultWorkEventCost(const double cost) {
  QSettings settings;
  settings.setValue(kDefaultWorkEventCostKey, cost);
}

QTime workDayStart() {
  QSettings settings;
  return settings.value(kWorkDayStartKey, defaultWorkDayStartValue()).toTime();
}

void setWorkDayStart(const QTime &time) {
  QSettings settings;
  settings.setValue(kWorkDayStartKey, time);
}

QTime workDayEnd() {
  QSettings settings;
  return settings.value(kWorkDayEndKey, defaultWorkDayEndValue()).toTime();
}

void setWorkDayEnd(const QTime &time) {
  QSettings settings;
  settings.setValue(kWorkDayEndKey, time);
}

int defaultSessionDurationMinutes() {
  QSettings settings;
  return settings
      .value(kDefaultSessionDurationMinutesKey, defaultSessionDurationMinutesValue())
      .toInt();
}

void setDefaultSessionDurationMinutes(const int minutes) {
  QSettings settings;
  settings.setValue(kDefaultSessionDurationMinutesKey, minutes);
}

} // namespace pcm::app_settings
