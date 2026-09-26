#pragma once

#include "provider_kind.h"

#include <QMetaType>
#include <QString>
#include <optional>

namespace pcm::meeting {

struct MeetingDescriptor {
  ProviderKind kind = ProviderKind::ExternalUrl;
  QString meetingRef;
  std::optional<QString> meetingUrl;
  std::optional<QString> invitationState;
};

} // namespace pcm::meeting

Q_DECLARE_METATYPE(pcm::meeting::MeetingDescriptor)
