#pragma once

#include "database.h"

#include <QLabel>
#include <QWidget>

#include <memory>

class QCustomPlot;

class AnalyticsPage final : public QWidget {
  Q_OBJECT

public:
  explicit AnalyticsPage(std::shared_ptr<pcm::database::Database> db,
                         QWidget *parent = nullptr);

  void refresh();

private:
  struct SummaryCard {
    QLabel *value = nullptr;
    QLabel *caption = nullptr;
  };

  void buildUi();
  void initPlot(QCustomPlot *plot, const QString &placeholderText) const;
  void updateSummaryCards(const pcm::database::DashboardSummary &summary) const;
  void updateIncomePlot(const std::vector<pcm::database::DashboardMonthlyStats> &stats) const;
  void updateMixPlot(const std::vector<pcm::database::DashboardMonthlyStats> &stats) const;

  std::shared_ptr<pcm::database::Database> mDb;
  SummaryCard mClientsCard;
  SummaryCard mActiveClientsCard;
  SummaryCard mSessionsCard;
  SummaryCard mIncomeCard;
  QCustomPlot *mIncomePlot = nullptr;
  QCustomPlot *mMixPlot = nullptr;
};
