#include "external_url_meeting_provider.h"

namespace pcm::meeting {

void ExternalUrlMeetingProvider::create(const MeetingCreateRequest &request) {
  const auto trimmedUrl = request.rawMeetingUrl.trimmed();

  MeetingDescriptor descriptor;
  descriptor.kind = ProviderKind::ExternalUrl;
  descriptor.meetingRef = trimmedUrl;
  descriptor.meetingUrl = trimmedUrl.isEmpty() ? std::nullopt : std::make_optional(trimmedUrl);
  descriptor.invitationState = std::nullopt;

  emit created(descriptor);
}

void ExternalUrlMeetingProvider::cancel(const QString &meetingRef) {
  Q_UNUSED(meetingRef);
  emit canceled();
}

} // namespace pcm::meeting
