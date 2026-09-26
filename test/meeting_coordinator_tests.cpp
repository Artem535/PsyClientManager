#include "meeting_coordinator.h"
#include "meeting_provider_test_listener.h"

#include <gtest/gtest.h>

using pcm::meeting::test::MeetingSignalListener;

TEST(MeetingCoordinatorTest, DispatchesCreateToExternalUrlProvider) {
  pcm::meeting::MeetingCoordinator coordinator;
  MeetingSignalListener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreated, &listener,
                    &MeetingSignalListener::onCreated);
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreateFailed, &listener,
                    &MeetingSignalListener::onCreateFailed);

  coordinator.createMeeting(pcm::meeting::ProviderKind::ExternalUrl,
                            {.rawMeetingUrl = "https://meet.example.invalid/room-9"});

  ASSERT_TRUE(listener.lastDescriptor.has_value());
  EXPECT_FALSE(listener.lastCreateError.has_value());
  EXPECT_EQ(listener.lastDescriptor->kind, pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(listener.lastDescriptor->meetingRef, QStringLiteral("https://meet.example.invalid/room-9"));
}

TEST(MeetingCoordinatorTest, DispatchesCreateToLiveKitProviderWhichFails) {
  pcm::meeting::MeetingCoordinator coordinator;
  MeetingSignalListener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreated, &listener,
                    &MeetingSignalListener::onCreated);
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreateFailed, &listener,
                    &MeetingSignalListener::onCreateFailed);

  coordinator.createMeeting(pcm::meeting::ProviderKind::LiveKit, {.rawMeetingUrl = ""});

  EXPECT_FALSE(listener.lastDescriptor.has_value());
  ASSERT_TRUE(listener.lastCreateError.has_value());
  EXPECT_FALSE(listener.lastCreateError->isEmpty());
}

TEST(MeetingCoordinatorTest, DispatchesCancelToExternalUrlProvider) {
  pcm::meeting::MeetingCoordinator coordinator;
  MeetingSignalListener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCanceled, &listener,
                    &MeetingSignalListener::onCanceled);

  coordinator.cancelMeeting(pcm::meeting::ProviderKind::ExternalUrl,
                            "https://meet.example.invalid/room-9");

  EXPECT_TRUE(listener.cancelSucceeded);
}

TEST(MeetingCoordinatorTest, DispatchesCancelToLiveKitProviderWhichFails) {
  pcm::meeting::MeetingCoordinator coordinator;
  MeetingSignalListener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCancelFailed, &listener,
                    &MeetingSignalListener::onCancelFailed);

  coordinator.cancelMeeting(pcm::meeting::ProviderKind::LiveKit, "some-ref");

  ASSERT_TRUE(listener.lastCancelError.has_value());
  EXPECT_FALSE(listener.lastCancelError->isEmpty());
}

