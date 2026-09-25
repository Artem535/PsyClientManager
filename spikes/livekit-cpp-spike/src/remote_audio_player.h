#pragma once

#include <QAudioDevice>
#include <QAudioSink>
#include <QIODevice>
#include <QObject>

#include <atomic>
#include <memory>
#include <thread>

#include "livekit/livekit.h"

// Plays a subscribed remote audio track through a QAudioSink. Runs its own
// reader thread blocking on livekit::AudioStream::read() (pull-based, same
// as RemoteVideoRenderer) and writes PCM into the sink's QIODevice.
class RemoteAudioPlayer final : public QObject {
  Q_OBJECT

public:
  explicit RemoteAudioPlayer(QObject *parent = nullptr);
  ~RemoteAudioPlayer() override;

  void attachTrack(const std::shared_ptr<livekit::Track> &track, const QAudioDevice &outputDevice);
  void detach();

private:
  std::shared_ptr<livekit::AudioStream> mStream;
  std::unique_ptr<QAudioSink> mSink;
  QIODevice *mSinkDevice{nullptr};
  std::thread mReaderThread;
  std::atomic<bool> mRunning{false};

  void readerLoop();
};
