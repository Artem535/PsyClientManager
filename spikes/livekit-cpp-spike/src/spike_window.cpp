#include "spike_window.h"

#include <QHBoxLayout>
#include <QMediaDevices>
#include <QMetaObject>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <iostream>

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

  mConnectionLabel = new QLabel("Not connected.", central);
  layout->addWidget(mConnectionLabel);

  auto *buttonRow = new QWidget(central);
  auto *buttonLayout = new QHBoxLayout(buttonRow);
  mJoinButton = new QPushButton("Join", buttonRow);
  mLeaveButton = new QPushButton("Leave", buttonRow);
  mLeaveButton->setEnabled(false);
  buttonLayout->addWidget(mJoinButton);
  buttonLayout->addWidget(mLeaveButton);
  layout->addWidget(buttonRow);

  connect(mJoinButton, &QPushButton::clicked, this, &SpikeWindow::onJoinClicked);
  connect(mLeaveButton, &QPushButton::clicked, this, &SpikeWindow::onLeaveClicked);

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

SpikeWindow::~SpikeWindow() { onLeaveClicked(); }

void SpikeWindow::setConnectionState(const QString &text) {
  mConnectionLabel->setText(text);
}

void SpikeWindow::onJoinClicked() {
  const char *url = std::getenv("LIVEKIT_URL");
  const char *token = std::getenv("LIVEKIT_TOKEN");
  if (!url || !token) {
    setConnectionState("LIVEKIT_URL / LIVEKIT_TOKEN not set — cannot join.");
    return;
  }

  mRoom = std::make_unique<livekit::Room>();
  mRoom->setDelegate(this);

  livekit::RoomOptions options;
  options.auto_subscribe = true;
  options.dynacast = false;

  setConnectionState("Connecting...");
  const bool connected = mRoom->connect(url, token, options);
  if (!connected) {
    setConnectionState("Failed to connect.");
    mRoom->setDelegate(nullptr);
    mRoom.reset();
    return;
  }

  setConnectionState("Connected.");
  mJoinButton->setEnabled(false);
  mLeaveButton->setEnabled(true);
  publishTracks();
}

void SpikeWindow::publishTracks() {
  auto lp = mRoom->localParticipant().lock();
  if (!lp) {
    std::cerr << "[room] local participant unavailable, cannot publish" << std::endl;
    return;
  }

  mAudioTrack = livekit::LocalAudioTrack::createLocalAudioTrack("mic", mAudioCapture.audioSource());
  livekit::TrackPublishOptions audioOpts;
  audioOpts.source = livekit::TrackSource::SOURCE_MICROPHONE;
  audioOpts.dtx = false;
  audioOpts.simulcast = false;
  try {
    lp->publishTrack(mAudioTrack, audioOpts);
  } catch (const std::exception &e) {
    std::cerr << "[room] failed to publish audio track: " << e.what() << std::endl;
  }

  mVideoTrack = livekit::LocalVideoTrack::createLocalVideoTrack("cam", mVideoCapture.videoSource());
  livekit::TrackPublishOptions videoOpts;
  videoOpts.source = livekit::TrackSource::SOURCE_CAMERA;
  videoOpts.dtx = false;
  videoOpts.simulcast = true;
  try {
    lp->publishTrack(mVideoTrack, videoOpts);
  } catch (const std::exception &e) {
    std::cerr << "[room] failed to publish video track: " << e.what() << std::endl;
  }
}

void SpikeWindow::unpublishTracks() {
  if (mRoom) {
    if (auto lp = mRoom->localParticipant().lock()) {
      if (mAudioTrack && mAudioTrack->publication()) {
        lp->unpublishTrack(mAudioTrack->publication()->sid());
      }
      if (mVideoTrack && mVideoTrack->publication()) {
        lp->unpublishTrack(mVideoTrack->publication()->sid());
      }
    }
  }
  mAudioTrack.reset();
  mVideoTrack.reset();
}

void SpikeWindow::onLeaveClicked() {
  if (!mRoom) {
    return;
  }
  unpublishTracks();
  mRoom->setDelegate(nullptr);
  mRoom.reset();

  mJoinButton->setEnabled(true);
  mLeaveButton->setEnabled(false);
  setConnectionState("Not connected.");
}

void SpikeWindow::onParticipantConnected(livekit::Room & /*room*/,
                                         const livekit::ParticipantConnectedEvent &ev) {
  const QString identity = ev.participant ? QString::fromStdString(ev.participant->identity())
                                          : QStringLiteral("<unknown>");
  QMetaObject::invokeMethod(
      this,
      [this, identity]() {
        setConnectionState(QStringLiteral("Connected. Participant joined: %1").arg(identity));
      },
      Qt::QueuedConnection);
}

void SpikeWindow::onTrackSubscribed(livekit::Room & /*room*/,
                                    const livekit::TrackSubscribedEvent & /*ev*/) {
  // Task 5 attaches the remote video/audio renderer here.
}
