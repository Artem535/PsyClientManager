#pragma once

#include "recurrence_utils.h"

#include <QLabel>
#include <QWidget>

class AppointmentSummaryWidget final : public QWidget {
  Q_OBJECT

public:
  explicit AppointmentSummaryWidget(QWidget *parent = nullptr);

  void setAppointments(const pcm::recurrence::LastNextAppointment &summary);
  void clear();

private:
  QLabel *mLastLabel = nullptr;
  QLabel *mNextLabel = nullptr;
};
