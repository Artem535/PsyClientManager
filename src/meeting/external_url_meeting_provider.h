#pragma once

#include "meeting_provider.h"

namespace pcm::meeting {

class ExternalUrlMeetingProvider final : public MeetingProvider {
  Q_OBJECT

public:
  using MeetingProvider::MeetingProvider;

  void create(const MeetingCreateRequest &request) override;
  void cancel(const QString &meetingRef) override;
};

} // namespace pcm::meeting
