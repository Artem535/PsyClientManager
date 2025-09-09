#pragma once

#include "database.h"
#include "qevent_details_widget.h"
#include "timeline_widget.h"
#include <QWidget>
#include <memory>

namespace Ui {
class EventInfo;
}

class QEventInfoPage final : public QWidget {
  Q_OBJECT

public:
  explicit QEventInfoPage(const std::shared_ptr<pcm::database::Database> &db,
                          QWidget *parent = nullptr);
  ~QEventInfoPage() override;

signals:
  void provideClientEventPairSave(obx_id clientId, obx_id eventId);

private slots:
  void onCalendarClicked(const QDate &date);
  void onTimelineEventSelected(QEventItem *event);
  void onEventSaved(QEventItem *event);
  void onEditingCanceled();

private:
  void connectSignals();
  void initDefaultStates();

  std::unique_ptr<Ui::EventInfo> mUi;
  std::shared_ptr<pcm::database::Database> mDb;
  QTimelineWidget *mTimelineWidget = nullptr;
  QEventDetailsWidget *mEventDetailsWidget = nullptr;
};