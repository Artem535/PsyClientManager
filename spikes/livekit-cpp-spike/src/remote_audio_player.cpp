#include "remote_audio_player.h"

#include <QByteArray>

#include <cstdint>
#include <iostream>

RemoteAudioPlayer::RemoteAudioPlayer(QObject *parent) : QObject(parent) {}

RemoteAudioPlayer::~RemoteAudioPlayer() { detach(); }

void RemoteAudioPlayer::attachTrack(const std::shared_ptr<livekit::Track> &track,
                                    const QAudioDevice &outputDevice) {
  detach();
  if (!track) {
    return;
  }

  livekit::AudioStream::Options opts;
  mStream = livekit::AudioStream::fromTrack(track, opts);
  if (!mStream) {
    std::cerr << "[remote audio] AudioStream::fromTrack failed" << std::endl;
    return;
  }

  QAudioFormat format;
  format.setSampleRate(48000);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);

  mSink = std::make_unique<QAudioSink>(outputDevice, format, this);
  mSinkDevice = mSink->start();
  if (!mSinkDevice) {
    std::cerr << "[remote audio] QAudioSink::start() returned null" << std::endl;
    return;
  }

  mRunning.store(true);
  mReaderThread = std::thread(&RemoteAudioPlayer::readerLoop, this);
}

void RemoteAudioPlayer::detach() {
  mRunning.store(false);
  if (mStream) {
    mStream->close();  // wakes a blocking read() so the reader thread can exit promptly
  }
  if (mReaderThread.joinable()) {
    mReaderThread.join();
  }
  if (mSink) {
    mSink->stop();
    mSink.reset();
  }
  mSinkDevice = nullptr;
  mStream.reset();
}

void RemoteAudioPlayer::readerLoop() {
  while (mRunning.load()) {
    livekit::AudioFrameEvent afe;
    if (!mStream->read(afe)) {
      break;
    }
    const auto &pcm = afe.frame.data();
    if (pcm.empty() || !mSinkDevice) {
      continue;
    }
    const QByteArray bytes(reinterpret_cast<const char *>(pcm.data()),
                           static_cast<qsizetype>(pcm.size() * sizeof(int16_t)));
    mSinkDevice->write(bytes);
  }
}
