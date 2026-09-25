#pragma once

#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QObject>
#include <QVideoSink>

#include <atomic>
#include <memory>

#include "livekit/livekit.h"

// Drives a QCamera through Qt Multimedia and pushes every captured frame,
// converted to RGBA, into a livekit::VideoSource. Owns no LiveKit track —
// callers publish videoSource() themselves (see spike_window.cpp).
class VideoCaptureAdapter final : public QObject {
  Q_OBJECT

public:
  explicit VideoCaptureAdapter(QObject *parent = nullptr);
  ~VideoCaptureAdapter() override;

  [[nodiscard]] std::shared_ptr<livekit::VideoSource> videoSource() const { return mVideoSource; }
  [[nodiscard]] QVideoSink *previewSink() { return &mSink; }
  [[nodiscard]] int framesCaptured() const { return mFramesCaptured.load(); }

  // Starts (or restarts, if already running) capture on the given device.
  void start(const QCameraDevice &device);
  void stop();

signals:
  void frameCaptured();

private slots:
  void onVideoFrameChanged(const QVideoFrame &frame);

private:
  std::shared_ptr<livekit::VideoSource> mVideoSource;
  std::unique_ptr<QCamera> mCamera;
  QMediaCaptureSession mSession;
  QVideoSink mSink;
  std::atomic<int> mFramesCaptured{0};
};
