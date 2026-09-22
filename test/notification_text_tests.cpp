#include <gtest/gtest.h>

#include <QDateTime>
#include <QLocale>

#include "notification_text.h"

namespace {

pcm::NotificationEventInfo makeInfo(const QString &title, const bool isWorkEvent,
                                    const QString &clientName) {
  pcm::NotificationEventInfo info;
  info.title = title;
  info.startTime = QDateTime(QDate(2026, 9, 22), QTime(18, 0));
  info.isWorkEvent = isWorkEvent;
  info.clientName = clientName;
  return info;
}

} // namespace

TEST(NotificationTextTest, FullModeIncludesTitleTimeAndClientForWorkEvents) {
  const auto info = makeInfo(QStringLiteral("Session"), true, QStringLiteral("Jane Doe"));
  EXPECT_EQ(pcm::notificationTitle(pcm::NotificationPrivacyMode::Full),
            QStringLiteral("Upcoming session"));
  const auto body = pcm::notificationBody(pcm::NotificationPrivacyMode::Full, info);
  EXPECT_TRUE(body.contains(QStringLiteral("Session")));
  EXPECT_TRUE(body.contains(QLocale().toString(info.startTime, "dd.MM.yyyy HH:mm")));
  EXPECT_TRUE(body.contains(QStringLiteral("Jane Doe")));
}

TEST(NotificationTextTest, FullModeOmitsClientForNonWorkEvents) {
  const auto info = makeInfo(QStringLiteral("Personal"), false, QStringLiteral("Jane Doe"));
  const auto body = pcm::notificationBody(pcm::NotificationPrivacyMode::Full, info);
  EXPECT_FALSE(body.contains(QStringLiteral("Jane Doe")));
}

TEST(NotificationTextTest, FullModeOmitsClientLineWhenNameIsEmpty) {
  const auto info = makeInfo(QStringLiteral("Session"), true, QString());
  const auto body = pcm::notificationBody(pcm::NotificationPrivacyMode::Full, info);
  EXPECT_FALSE(body.contains(QStringLiteral("Client")));
}

TEST(NotificationTextTest, HiddenModeNeverLeaksTitleOrClient) {
  const auto info = makeInfo(QStringLiteral("Very Private Topic"), true, QStringLiteral("Jane Doe"));
  EXPECT_EQ(pcm::notificationTitle(pcm::NotificationPrivacyMode::Hidden),
            QStringLiteral("PsyClientManager"));
  const auto body = pcm::notificationBody(pcm::NotificationPrivacyMode::Hidden, info);
  EXPECT_EQ(body, QStringLiteral("Scheduled session"));
  EXPECT_FALSE(body.contains(QStringLiteral("Jane Doe")));
  EXPECT_FALSE(body.contains(QStringLiteral("Very Private Topic")));
}

TEST(NotificationTextTest, MinimalModeShowsOnlyTime) {
  const auto info = makeInfo(QStringLiteral("Very Private Topic"), true, QStringLiteral("Jane Doe"));
  const auto body = pcm::notificationBody(pcm::NotificationPrivacyMode::Minimal, info);
  EXPECT_EQ(body, QLocale().toString(info.startTime, "HH:mm"));
  EXPECT_FALSE(body.contains(QStringLiteral("Jane Doe")));
  EXPECT_FALSE(body.contains(QStringLiteral("Very Private Topic")));
}
