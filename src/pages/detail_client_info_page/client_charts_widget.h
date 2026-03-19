#pragma once

#include "database.h"

#include <QWidget>

class QCustomPlot;

class ClientChartsWidget final : public QWidget {
  Q_OBJECT

public:
  explicit ClientChartsWidget(QWidget *parent = nullptr);

  void setMonthlyStats(const std::vector<pcm::database::ClientMonthlyStats> &stats);
  void clear();

private:
  void initPlot();
  void updatePlaceholder(bool hasData);

  QCustomPlot *mPlot = nullptr;
};
