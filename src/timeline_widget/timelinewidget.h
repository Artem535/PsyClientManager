#pragma once
#include "eventdatamanager.h"
#include "eventitem.h"
#include "eventview.h"
#include <QLoggingCategory>
#include <QObject>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <qboxlayout.h>
#include <qlogging.h>
#include <qloggingcategory.h>
#include <qtmetamacros.h>

class QTimelineWidget final : public QWidget {
  Q_OBJECT
public:
  explicit QTimelineWidget(const std::shared_ptr<pcm::database::Database> &db,
                          QWidget *parent = nullptr);
  ~QTimelineWidget() override;

public slots:
  void onSelectedDayChanged(const QDate &date) const;
  void addEvent(QEventItem *item) const;

signals:
  void eventSelected(QEventItem *event);

private slots:
  void onEventSelected(QEventItem *event);
  void addEvent(const ObxEvent &event) const;

private:
  QVBoxLayout *mLayout;
  QEventView *mEventView;
  QEventDataManager *mDataManager;
};