#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimeZone>
#include <QDateTime>

#include <memory>

#include "event_item.h"
#include "event_view.h"
#include "qevent_data_manager.h"

class QTimelineWidget final : public QWidget {
  Q_OBJECT
public:
  explicit QTimelineWidget(const std::shared_ptr<pcm::database::Database> &db,
                           QWidget *parent = nullptr);
  ~QTimelineWidget() override;

public slots:
  void onSelectedDayChanged(const QDate &date) const;
  [[nodiscard]] obx_id addEvent(QEventItem *item) const;
  void updateScene();

signals:
  void eventSelected(QEventItem *event);
  void needSceneUpdate();

private slots:
  void onEventSelected(QEventItem *event);
  void addEvent(const ObxEvent &event) const;

private:
  QVBoxLayout *mLayout;
  QEventView *mEventView;
  QEventDataManager *mDataManager;
};