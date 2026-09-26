#include "external_url_meeting_provider.h"
#include "meeting_provider_test_listener.h"

#include <gtest/gtest.h>

using pcm::meeting::test::MeetingSignalListener;

TEST(ExternalUrlMeetingProviderTest, CreateEmitsDescriptorMatchingTheGivenUrl) {
  pcm::meeting::ExternalUrlMeetingProvider provider;
  MeetingSignalListener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::created, &listener,
                    &MeetingSignalListener::onCreated);
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::createFailed, &listener,
                    &MeetingSignalListener::onCreateFailed);

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
  MeetingSignalListener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::created, &listener,
                    &MeetingSignalListener::onCreated);

  provider.create({.rawMeetingUrl = "   "});

  ASSERT_TRUE(listener.lastDescriptor.has_value());
  EXPECT_EQ(listener.lastDescriptor->kind, pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(listener.lastDescriptor->meetingRef, QStringLiteral(""));
  EXPECT_FALSE(listener.lastDescriptor->meetingUrl.has_value());
}

TEST(ExternalUrlMeetingProviderTest, CancelAlwaysSucceeds) {
  pcm::meeting::ExternalUrlMeetingProvider provider;
  MeetingSignalListener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::canceled, &listener,
                    &MeetingSignalListener::onCanceled);

  provider.cancel("https://meet.example.invalid/room-1");

  EXPECT_TRUE(listener.cancelSucceeded);
}

