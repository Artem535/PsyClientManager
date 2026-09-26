#pragma once

#include "meeting_provider.h"

namespace pcm::meeting {

// Non-functional in this version: satisfies MeetingProvider but never makes a
// network call. LiveKit is not yet selectable anywhere in the UI; the real
// token-backend integration lands with the native call UI.
class LiveKitMeetingProvider final : public MeetingProvider {
  Q_OBJECT

public:
  using MeetingProvider::MeetingProvider;

  void create(const MeetingCreateRequest &request) override;
  void cancel(const QString &meetingRef) override;
};

} // namespace pcm::meeting
