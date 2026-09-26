#include "livekit_meeting_provider.h"

#include <gtest/gtest.h>

namespace {

class Listener : public QObject {
  Q_OBJECT
public:
  bool createSucceeded = false;
  std::optional<QString> lastCreateError;
  bool cancelSucceeded = false;
  std::optional<QString> lastCancelError;

public slots:
  void onCreated(pcm::meeting::MeetingDescriptor) { createSucceeded = true; }
  void onCreateFailed(QString error) { lastCreateError = error; }
  void onCanceled() { cancelSucceeded = true; }
  void onCancelFailed(QString error) { lastCancelError = error; }
};

} // namespace

TEST(LiveKitMeetingProviderTest, CreateAlwaysFailsWithoutClaimingSuccess) {
  pcm::meeting::LiveKitMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::created, &listener,
                    &Listener::onCreated);
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::createFailed, &listener,
                    &Listener::onCreateFailed);

  provider.create({.rawMeetingUrl = ""});

  EXPECT_FALSE(listener.createSucceeded);
  ASSERT_TRUE(listener.lastCreateError.has_value());
  EXPECT_FALSE(listener.lastCreateError->isEmpty());
}

TEST(LiveKitMeetingProviderTest, CancelAlwaysFailsWithoutClaimingSuccess) {
  pcm::meeting::LiveKitMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::canceled, &listener,
                    &Listener::onCanceled);
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::cancelFailed, &listener,
                    &Listener::onCancelFailed);

  provider.cancel("some-meeting-ref");

  EXPECT_FALSE(listener.cancelSucceeded);
  ASSERT_TRUE(listener.lastCancelError.has_value());
  EXPECT_FALSE(listener.lastCancelError->isEmpty());
}

#include "livekit_meeting_provider_tests.moc"
