#include "video_capture_adapter.h"

#include <QMediaDevices>

#include <iostream>

#include "frame_convert.h"

namespace {
constexpr int kVideoWidth = 1280;
constexpr int kVideoHeight = 720;
}  // namespace

VideoCaptureAdapter::VideoCaptureAdapter(QObject *parent)
    : QObject(parent),
      mVideoSource(std::make_shared<livekit::VideoSource>(kVideoWidth, kVideoHeight)) {
  mSession.setVideoSink(&mSink);
  connect(&mSink, &QVideoSink::videoFrameChanged, this,
          &VideoCaptureAdapter::onVideoFrameChanged);
}

VideoCaptureAdapter::~VideoCaptureAdapter() { stop(); }

void VideoCaptureAdapter::start(const QCameraDevice &device) {
  stop();
  mCamera = std::make_unique<QCamera>(device, nullptr);
  mSession.setCamera(mCamera.get());
  mCamera->start();
}

void VideoCaptureAdapter::stop() {
  if (mCamera) {
    mCamera->stop();
    mSession.setCamera(nullptr);
    mCamera.reset();
  }
}

void VideoCaptureAdapter::onVideoFrameChanged(const QVideoFrame &frame) {
  if (!frame.isValid()) {
    return;
  }
  const QImage image = frame.toImage();
  if (image.isNull()) {
    return;
  }

  const auto liveKitFrame = pcm::spike::videoFrameToLiveKitRGBA(image);
  try {
    mVideoSource->captureFrame(liveKitFrame, 0, livekit::VideoRotation::VIDEO_ROTATION_0);
    mFramesCaptured.fetch_add(1);
    emit frameCaptured();
  } catch (const std::exception &e) {
    std::cerr << "[video capture] captureFrame failed: " << e.what() << std::endl;
  }
}
