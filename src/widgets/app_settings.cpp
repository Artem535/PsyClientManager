#include "app_settings.h"

#include <QColor>
#include <QObject>
#include <QSettings>
#include <QTime>

namespace {
constexpr auto kConfirmEventDeletionKey = "event/confirmDeletion";
constexpr auto kPreventEventOverlapsKey = "event/preventOverlaps";
constexpr auto kShowStatusBarMessagesKey = "ui/showStatusBarMessages";
constexpr auto kNotificationsEnabledKey = "notification/enabled";
constexpr auto kNotificationLeadMinutesKey = "notification/leadMinutes";
constexpr auto kLanguageCodeKey = "ui/language";
constexpr auto kWorkEventColorKey = "timeline/workEventColor";
constexpr auto kPersonalEventColorKey = "timeline/personalEventColor";
constexpr auto kDefaultWorkEventCostKey = "event/defaultWorkEventCost";
constexpr auto kWorkDayStartKey = "event/workDayStart";
constexpr auto kWorkDayEndKey = "event/workDayEnd";
constexpr auto kDefaultSessionDurationMinutesKey = "event/defaultSessionDurationMinutes";
constexpr auto kMeetingInviteTemplateKey = "online/meetingInviteTemplate";

QColor defaultWorkEventColor() {
  return QColor(37, 99, 235);
}

QColor defaultPersonalEventColor() {
  return QColor(126, 34, 206);
}

QColor legacyDefaultWorkEventColor() {
  return QColor(173, 216, 230);
}

QColor legacyDefaultPersonalEventColor() {
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

int defaultNotificationLeadMinutesValue() {
  return 30;
}

QString defaultMeetingInviteTemplateValue() {
  return QObject::tr("Hello, {client_name}!\n\n"
                     "We meet on {date} at {time}.\n\n"
                     "Connection link:\n"
                     "{meeting_url}\n\n"
                     "See you!");
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

bool notificationsEnabled() {
  QSettings settings;
  return settings.value(kNotificationsEnabledKey, true).toBool();
}

void setNotificationsEnabled(const bool enabled) {
  QSettings settings;
  settings.setValue(kNotificationsEnabledKey, enabled);
}

int notificationLeadMinutes() {
  QSettings settings;
  return settings.value(kNotificationLeadMinutesKey,
                        defaultNotificationLeadMinutesValue())
      .toInt();
}

void setNotificationLeadMinutes(const int minutes) {
  QSettings settings;
  settings.setValue(kNotificationLeadMinutesKey, minutes);
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
  const auto color = settings.value(kWorkEventColorKey, defaultWorkEventColor()).value<QColor>();
  return color == legacyDefaultWorkEventColor() ? defaultWorkEventColor() : color;
}

void setWorkEventColor(const QColor &color) {
  QSettings settings;
  settings.setValue(kWorkEventColorKey, color);
}

QColor personalEventColor() {
  QSettings settings;
  const auto color =
      settings.value(kPersonalEventColorKey, defaultPersonalEventColor()).value<QColor>();
  return color == legacyDefaultPersonalEventColor() ? defaultPersonalEventColor() : color;
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

QString meetingInviteTemplate() {
  QSettings settings;
  return settings.value(kMeetingInviteTemplateKey,
                        defaultMeetingInviteTemplateValue())
      .toString();
}

void setMeetingInviteTemplate(const QString &templateText) {
  QSettings settings;
  settings.setValue(kMeetingInviteTemplateKey, templateText);
}

} // namespace pcm::app_settings
