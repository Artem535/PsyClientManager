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
  QEventInfoPage(QTimelineModel *model, QWidget *parent);
  ~QEventInfoPage() override;

signals:
  void provideClientEventPairSave(int64_t clientId, int64_t eventId);
  void provideFillClientComboBox(QComboBox *comboBox);
  void provideClientByEventId(int64_t eventId);
  void clientResolved(int64_t clientId);

public slots:
  void onClientResolved(int64_t clientId);

private slots:
  void onCalendarClicked(const QDate &date);
  void onTimelineEventSelected(QEventItem *event);
  void onEventSaved(QEventItem *event);
  void onEditingCanceled();

private:
  void connectSignals();
  void initDefaultStates();

  std::unique_ptr<Ui::EventInfo> mUi;
  QTimelineWidget *mTimelineWidget = nullptr;
  QEventDetailsWidget *mEventDetailsWidget = nullptr;

  int64_t mClientId = 0;
};
