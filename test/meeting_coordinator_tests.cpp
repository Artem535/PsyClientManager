#include "meeting_coordinator.h"

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

TEST(MeetingCoordinatorTest, DispatchesCreateToExternalUrlProvider) {
  pcm::meeting::MeetingCoordinator coordinator;
  Listener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreated, &listener,
                    &Listener::onCreated);
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreateFailed, &listener,
                    &Listener::onCreateFailed);

  coordinator.createMeeting(pcm::meeting::ProviderKind::ExternalUrl,
                            {.rawMeetingUrl = "https://meet.example.invalid/room-9"});

  ASSERT_TRUE(listener.lastDescriptor.has_value());
  EXPECT_FALSE(listener.lastCreateError.has_value());
  EXPECT_EQ(listener.lastDescriptor->kind, pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(listener.lastDescriptor->meetingRef, QStringLiteral("https://meet.example.invalid/room-9"));
}

TEST(MeetingCoordinatorTest, DispatchesCreateToLiveKitProviderWhichFails) {
  pcm::meeting::MeetingCoordinator coordinator;
  Listener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreated, &listener,
                    &Listener::onCreated);
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreateFailed, &listener,
                    &Listener::onCreateFailed);

  coordinator.createMeeting(pcm::meeting::ProviderKind::LiveKit, {.rawMeetingUrl = ""});

  EXPECT_FALSE(listener.lastDescriptor.has_value());
  ASSERT_TRUE(listener.lastCreateError.has_value());
  EXPECT_FALSE(listener.lastCreateError->isEmpty());
}

TEST(MeetingCoordinatorTest, DispatchesCancelToExternalUrlProvider) {
  pcm::meeting::MeetingCoordinator coordinator;
  Listener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCanceled, &listener,
                    &Listener::onCanceled);

  coordinator.cancelMeeting(pcm::meeting::ProviderKind::ExternalUrl,
                            "https://meet.example.invalid/room-9");

  EXPECT_TRUE(listener.cancelSucceeded);
}

TEST(MeetingCoordinatorTest, DispatchesCancelToLiveKitProviderWhichFails) {
  pcm::meeting::MeetingCoordinator coordinator;
  Listener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCancelFailed, &listener,
                    &Listener::onCancelFailed);

  coordinator.cancelMeeting(pcm::meeting::ProviderKind::LiveKit, "some-ref");

  ASSERT_TRUE(listener.lastCancelError.has_value());
  EXPECT_FALSE(listener.lastCancelError->isEmpty());
}

#include "meeting_coordinator_tests.moc"
