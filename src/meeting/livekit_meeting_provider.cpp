#include "livekit_meeting_provider.h"

namespace pcm::meeting {

namespace {
const QString kNotYetAvailable =
    QStringLiteral("LiveKit meetings are not yet available in this version.");
}

void LiveKitMeetingProvider::create(const MeetingCreateRequest &request) {
  Q_UNUSED(request);
  emit createFailed(kNotYetAvailable);
}

void LiveKitMeetingProvider::cancel(const QString &meetingRef) {
  Q_UNUSED(meetingRef);
  emit cancelFailed(kNotYetAvailable);
}

} // namespace pcm::meeting
