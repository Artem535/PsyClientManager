#pragma once

#include "meeting_provider.h"

#include <QObject>
#include <QString>
#include <optional>

namespace pcm::meeting::test {

// Shared MeetingProvider signal listener used by external_url_meeting_provider_tests.cpp
// and meeting_coordinator_tests.cpp, both of which exercise the created/createFailed/
// canceled/cancelFailed signal quartet against a descriptor-producing provider.
class MeetingSignalListener : public QObject {
  Q_OBJECT
public:
  std::optional<MeetingDescriptor> lastDescriptor;
  std::optional<QString> lastCreateError;
  bool cancelSucceeded = false;
  std::optional<QString> lastCancelError;

public slots:
  void onCreated(MeetingDescriptor descriptor) { lastDescriptor = descriptor; }
  void onCreateFailed(QString error) { lastCreateError = error; }
  void onCanceled() { cancelSucceeded = true; }
  void onCancelFailed(QString error) { lastCancelError = error; }
};

} // namespace pcm::meeting::test
