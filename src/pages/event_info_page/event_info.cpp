#include "event_info.h"
#include "ui/pages/ui_eventinfo.h"

#include <QDialog>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(logEventInfo, "pcm.EventInfo")

QEventInfoPage::QEventInfoPage(QTimelineModel *model, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()) {
  mUi->setupUi(this);

  mSelectedDate = QDate::currentDate();

  mTimelineWidget = new QTimelineWidget(model, this);
  mUi->list_view_v_layout->addWidget(mTimelineWidget);

  mCreateEventButton = new QPushButton(tr(": EVENT_ADD_BUTTON"), this);
  mUi->list_view_v_layout->insertWidget(0, mCreateEventButton);
  mUi->frame_2->setVisible(false);

  connectSignals();
  initDefaultStates();
}

QEventInfoPage::~QEventInfoPage() = default;

void QEventInfoPage::connectSignals() {
  // CalendarWidget -> TimelineWidget
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, mTimelineWidget,
          &QTimelineWidget::onSelectedDayChanged);
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, this,
          &QEventInfoPage::onCalendarClicked);
  connect(mCreateEventButton, &QPushButton::clicked, this,
          &QEventInfoPage::onCreateEventClicked);

  connect(mTimelineWidget, &QTimelineWidget::eventSelected, this,
          &QEventInfoPage::onTimelineEventSelected);

  connect(this, &QEventInfoPage::clientResolved, this,
          &QEventInfoPage::onClientResolved);
}

void QEventInfoPage::initDefaultStates() {
  mSelectedDate = QDate::currentDate();
  mUi->calendar_widget->setSelectedDate(mSelectedDate);
  mTimelineWidget->onSelectedDayChanged(mSelectedDate);
}

void QEventInfoPage::onCalendarClicked(const QDate &date) {
  mSelectedDate = date;
}

void QEventInfoPage::onCreateEventClicked() { openEventDialog(nullptr); }

void QEventInfoPage::openEventDialog(QEventItem *event,
                                     const std::optional<int64_t> clientId) {
  auto dialog = QDialog(this);
  dialog.setModal(true);
  dialog.setWindowTitle(event ? tr(": EVENT_EDIT_BUTTON")
                              : tr(": EVENT_ADD_BUTTON"));

  auto layout = QVBoxLayout(&dialog);
  layout.setContentsMargins(0, 0, 0, 0);

  auto *detailsWidget = new QEventDetailsWidget(&dialog);
  detailsWidget->setDialogMode(true);
  layout.addWidget(detailsWidget);

  mActiveEventDetailsWidget = detailsWidget;

  connect(detailsWidget, &QEventDetailsWidget::provideEventSave, this,
          &QEventInfoPage::onEventSaved);
  connect(detailsWidget, &QEventDetailsWidget::provideEditingCanceled, this,
          &QEventInfoPage::onEditingCanceled);
  connect(detailsWidget, &QEventDetailsWidget::provideDialogAccept, &dialog,
          &QDialog::accept);
  connect(detailsWidget, &QEventDetailsWidget::provideEditingCanceled, &dialog,
          &QDialog::reject);
  connect(detailsWidget, &QEventDetailsWidget::provideClientEventPairSave, this,
          [this](const int64_t selectedClientId, const int64_t selectedEventId) {
            emit provideClientEventPairSave(selectedClientId, selectedEventId);
          });
  connect(detailsWidget, &QEventDetailsWidget::provideFillClientComboBox, this,
          [this](QComboBox *comboBox) {
            emit provideFillClientComboBox(comboBox);
          });

  if (event) {
    detailsWidget->startEditingEvent(event, clientId);
  } else {
    detailsWidget->startCreatingNewEvent(mSelectedDate);
  }

  dialog.resize(460, 620);
  dialog.exec();

  mActiveEventDetailsWidget.clear();
}

void QEventInfoPage::onTimelineEventSelected(QEventItem *event) {
  if (!event)
    return;

  qCDebug(logEventInfo) << "Selected event with ID:" << event->getId();

  std::optional<int64_t> clientId{std::nullopt};
  if (event->isWorkItem()) {
    emit provideClientByEventId(event->getId());
    clientId = mClientId;
  }

  openEventDialog(event, clientId);
}

void QEventInfoPage::onEventSaved(QEventItem *event) {
  if (!event)
    return;
  qCDebug(logEventInfo) << "Event saved with ID:" << event->getId();

  auto eventDetails = event->toEvent();

  if (mActiveEventDetailsWidget && mActiveEventDetailsWidget->isCreatingNewEvent()) {
    const auto id = mTimelineWidget->addEvent(eventDetails);
    if (id <= 0) {
      qCWarning(logEventInfo) << "Failed to persist event in DB";
      return;
    }
    eventDetails.id = id;
    event->setId(id);
  } else {
    mTimelineWidget->updateEvent(eventDetails);
  }

  // Force reload from DB to avoid stale UI state.
  mTimelineWidget->onSelectedDayChanged(mSelectedDate);
}

void QEventInfoPage::onEditingCanceled() {}
void QEventInfoPage::onClientResolved(int64_t clientId) { mClientId = clientId; }
