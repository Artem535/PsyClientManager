#pragma once

#include "recurrence_utils.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class DaySummaryWidget final : public QWidget {
  Q_OBJECT

public:
  explicit DaySummaryWidget(QWidget *parent = nullptr);

public slots:
  void setSummary(const pcm::recurrence::DaySummary &summary);

signals:
  void eventHighlightRequested(int64_t eventId);

private:
  void clearMiniList();

  QLabel *mDateLabel = nullptr;
  QLabel *mCountsLabel = nullptr;
  QLabel *mBusyLabel = nullptr;
  QLabel *mNextSessionLabel = nullptr;
  QLabel *mFreeWindowLabel = nullptr;
  QVBoxLayout *mMiniListLayout = nullptr;
};
