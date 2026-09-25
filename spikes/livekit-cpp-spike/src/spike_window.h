#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QVideoWidget>

#include <memory>

#include "audio_capture_adapter.h"
#include "video_capture_adapter.h"

// Minimal window for the LiveKit spike. This task only wires up local
// capture + preview; Task 4 adds room connection, Task 5 adds remote
// rendering, Task 6 adds device switching and teardown polish.
class SpikeWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit SpikeWindow(QWidget *parent = nullptr);

private:
  VideoCaptureAdapter mVideoCapture;
  AudioCaptureAdapter mAudioCapture;
  QVideoWidget *mLocalPreview{nullptr};
  QLabel *mStatusLabel{nullptr};

  void updateStatusLabel();
};
