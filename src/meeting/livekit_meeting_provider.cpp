#include "livekit_meeting_provider.h"

namespace pcm::meeting {

void LiveKitMeetingProvider::create(const MeetingCreateRequest &request) {
  Q_UNUSED(request);
  emit createFailed(
      QStringLiteral("LiveKit meetings are not yet available in this version."));
}

void LiveKitMeetingProvider::cancel(const QString &meetingRef) {
  Q_UNUSED(meetingRef);
  emit cancelFailed(
      QStringLiteral("LiveKit meetings are not yet available in this version."));
}

} // namespace pcm::meeting
