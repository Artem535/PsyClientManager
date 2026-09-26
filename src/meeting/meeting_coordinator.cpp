#include "meeting_coordinator.h"

namespace pcm::meeting {

MeetingCoordinator::MeetingCoordinator(QObject *parent)
    : QObject(parent), mExternalUrlProvider(new ExternalUrlMeetingProvider(this)),
      mLiveKitProvider(new LiveKitMeetingProvider(this)) {
  for (auto *provider : {static_cast<MeetingProvider *>(mExternalUrlProvider),
                         static_cast<MeetingProvider *>(mLiveKitProvider)}) {
    connect(provider, &MeetingProvider::created, this, &MeetingCoordinator::meetingCreated);
    connect(provider, &MeetingProvider::createFailed, this,
            &MeetingCoordinator::meetingCreateFailed);
    connect(provider, &MeetingProvider::canceled, this, &MeetingCoordinator::meetingCanceled);
    connect(provider, &MeetingProvider::cancelFailed, this,
            &MeetingCoordinator::meetingCancelFailed);
  }
}

MeetingProvider *MeetingCoordinator::providerFor(const ProviderKind kind) const {
  switch (kind) {
  case ProviderKind::ExternalUrl:
    return mExternalUrlProvider;
  case ProviderKind::LiveKit:
    return mLiveKitProvider;
  }
  return mExternalUrlProvider;
}

void MeetingCoordinator::createMeeting(const ProviderKind kind, const MeetingCreateRequest &request) {
  providerFor(kind)->create(request);
}

void MeetingCoordinator::cancelMeeting(const ProviderKind kind, const QString &meetingRef) {
  providerFor(kind)->cancel(meetingRef);
}

} // namespace pcm::meeting
