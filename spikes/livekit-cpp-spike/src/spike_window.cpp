#include "spike_window.h"

#include <QMediaDevices>
#include <QVBoxLayout>
#include <QWidget>

SpikeWindow::SpikeWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("LiveKit C++/Qt Spike");
  resize(960, 640);

  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);

  mLocalPreview = new QVideoWidget(central);
  mLocalPreview->setMinimumSize(640, 360);
  layout->addWidget(mLocalPreview);

  mStatusLabel = new QLabel(central);
  layout->addWidget(mStatusLabel);

  setCentralWidget(central);

  // Show the local capture in this window's own preview, independent of
  // what we push into LiveKit's VideoSource — proves capture works even
  // before Task 4 wires up a room connection.
  //
  // Deliberately NOT calling mLocalPreview->videoSink()->disconnect() here:
  // QVideoWidget wires its own internal repaint slot to its own videoSink()'s
  // videoFrameChanged signal in its constructor. A blanket disconnect() (no
  // args) on that sink severs every outgoing connection from it, including
  // that internal repaint wiring — frames would still flow into the sink via
  // setVideoFrame() below, but the widget would never repaint, so the
  // preview renders blank even while frame counters keep climbing. Qt
  // signals support multiple listeners, so no disconnect is needed: we just
  // add our own frame-forwarding connection alongside the widget's existing
  // internal one.
  connect(mVideoCapture.previewSink(), &QVideoSink::videoFrameChanged,
          mLocalPreview->videoSink(), &QVideoSink::setVideoFrame);

  connect(&mVideoCapture, &VideoCaptureAdapter::frameCaptured, this,
          &SpikeWindow::updateStatusLabel);
  connect(&mAudioCapture, &AudioCaptureAdapter::frameCaptured, this,
          &SpikeWindow::updateStatusLabel);

  const auto cameras = QMediaDevices::videoInputs();
  if (!cameras.isEmpty()) {
    mVideoCapture.start(cameras.first());
  } else {
    mStatusLabel->setText("No camera device found.");
  }

  const auto mics = QMediaDevices::audioInputs();
  if (!mics.isEmpty()) {
    mAudioCapture.start(mics.first());
  }

  updateStatusLabel();
}

void SpikeWindow::updateStatusLabel() {
  mStatusLabel->setText(QStringLiteral("Video frames captured: %1   Audio frames captured: %2")
                            .arg(mVideoCapture.framesCaptured())
                            .arg(mAudioCapture.framesCaptured()));
}
