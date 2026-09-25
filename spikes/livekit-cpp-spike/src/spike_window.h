#pragma once

#include <QComboBox>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVideoWidget>

#include <memory>

#include "audio_capture_adapter.h"
#include "livekit/livekit.h"
#include "remote_audio_player.h"
#include "remote_video_renderer.h"
#include "video_capture_adapter.h"

class SpikeWindow final : public QMainWindow, public livekit::RoomDelegate {
  Q_OBJECT

public:
  explicit SpikeWindow(QWidget *parent = nullptr);
  ~SpikeWindow() override;

  // livekit::RoomDelegate overrides
  void onParticipantConnected(livekit::Room &room,
                              const livekit::ParticipantConnectedEvent &ev) override;
  void onTrackSubscribed(livekit::Room &room,
                         const livekit::TrackSubscribedEvent &ev) override;

private slots:
  void onJoinClicked();
  void onLeaveClicked();

private:
  VideoCaptureAdapter mVideoCapture;
  AudioCaptureAdapter mAudioCapture;
  QVideoWidget *mLocalPreview{nullptr};
  RemoteVideoRenderer *mRemoteVideo{nullptr};
  RemoteAudioPlayer mRemoteAudio;
  QLabel *mStatusLabel{nullptr};
  QLabel *mConnectionLabel{nullptr};
  QPushButton *mJoinButton{nullptr};
  QPushButton *mLeaveButton{nullptr};
  QComboBox *mCameraCombo{nullptr};
  QComboBox *mMicCombo{nullptr};
  QComboBox *mSpeakerCombo{nullptr};

  std::unique_ptr<livekit::Room> mRoom;
  std::shared_ptr<livekit::LocalAudioTrack> mAudioTrack;
  std::shared_ptr<livekit::LocalVideoTrack> mVideoTrack;

  void updateStatusLabel();
  void setConnectionState(const QString &text);
  void publishTracks();
  void unpublishTracks();
};
