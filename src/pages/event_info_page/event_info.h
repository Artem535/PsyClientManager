#pragma once

#include "database.h"
#include "qevent_details_widget.h"
#include "timeline_widget.h"
#include "../../widgets/rounded_calendar_widget.h"
#include <QDate>
#include <QPointer>
#include <QPushButton>
#include <QWidget>
#include <memory>
#include <optional>

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
  void onCreateEventClicked();
  void onTimelineEventSelected(QEventItem *event);
  void onTimelineEventEditRequested(QEventItem *event);
  void onTimelineEventDeleteRequested(int64_t eventId);
  void onEventSaved(QEventItem *event);
  void onEditingCanceled();

private:
  void connectSignals();
  void initDefaultStates();
  void updateCalendarHighlights() const;
  void openEventDialog(QEventItem *event,
                       std::optional<int64_t> clientId = std::nullopt);
  void editEventWithDialog(QEventItem *event);

  std::unique_ptr<Ui::EventInfo> mUi;
  RoundedCalendarWidget *mCalendarWidget = nullptr;
  QTimelineWidget *mTimelineWidget = nullptr;
  QPushButton *mCreateEventButton = nullptr;
  QPointer<QEventDetailsWidget> mActiveEventDetailsWidget;

  int64_t mClientId = 0;
  QDate mSelectedDate;
};
