#include "external_url_meeting_provider.h"

#include <gtest/gtest.h>

namespace {

class Listener : public QObject {
  Q_OBJECT
public:
  std::optional<pcm::meeting::MeetingDescriptor> lastDescriptor;
  std::optional<QString> lastCreateError;
  bool cancelSucceeded = false;
  std::optional<QString> lastCancelError;

public slots:
  void onCreated(pcm::meeting::MeetingDescriptor descriptor) { lastDescriptor = descriptor; }
  void onCreateFailed(QString error) { lastCreateError = error; }
  void onCanceled() { cancelSucceeded = true; }
  void onCancelFailed(QString error) { lastCancelError = error; }
};

} // namespace

TEST(ExternalUrlMeetingProviderTest, CreateEmitsDescriptorMatchingTheGivenUrl) {
  pcm::meeting::ExternalUrlMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::created, &listener,
                    &Listener::onCreated);
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::createFailed, &listener,
                    &Listener::onCreateFailed);

  provider.create({.rawMeetingUrl = "https://meet.example.invalid/room-1"});

  ASSERT_TRUE(listener.lastDescriptor.has_value());
  EXPECT_FALSE(listener.lastCreateError.has_value());
  EXPECT_EQ(listener.lastDescriptor->kind, pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(listener.lastDescriptor->meetingRef, QStringLiteral("https://meet.example.invalid/room-1"));
  ASSERT_TRUE(listener.lastDescriptor->meetingUrl.has_value());
  EXPECT_EQ(*listener.lastDescriptor->meetingUrl, QStringLiteral("https://meet.example.invalid/room-1"));
  EXPECT_FALSE(listener.lastDescriptor->invitationState.has_value());
}

TEST(ExternalUrlMeetingProviderTest, CreateWithEmptyUrlStillEmitsADescriptor) {
  pcm::meeting::ExternalUrlMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::created, &listener,
                    &Listener::onCreated);

  provider.create({.rawMeetingUrl = "   "});

  ASSERT_TRUE(listener.lastDescriptor.has_value());
  EXPECT_EQ(listener.lastDescriptor->kind, pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(listener.lastDescriptor->meetingRef, QStringLiteral(""));
  EXPECT_FALSE(listener.lastDescriptor->meetingUrl.has_value());
}

TEST(ExternalUrlMeetingProviderTest, CancelAlwaysSucceeds) {
  pcm::meeting::ExternalUrlMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::canceled, &listener,
                    &Listener::onCanceled);

  provider.cancel("https://meet.example.invalid/room-1");

  EXPECT_TRUE(listener.cancelSucceeded);
}

#include "external_url_meeting_provider_tests.moc"
