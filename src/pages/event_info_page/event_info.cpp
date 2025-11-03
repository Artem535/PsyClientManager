#include "event_info.h"
#include "ui/pages/ui_eventinfo.h"

#include <QDateEdit>

Q_LOGGING_CATEGORY(logEventInfo, "pcm.EventInfo")

QEventInfoPage::QEventInfoPage(QTimelineModel *model, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()) {
  mUi->setupUi(this);

  mTimelineWidget = new QTimelineWidget(model, this);
  mUi->list_view_v_layout->addWidget(mTimelineWidget);

  mEventDetailsWidget = new QEventDetailsWidget(this);
  mUi->detailsContainer->layout()->addWidget(mEventDetailsWidget);
  mEventDetailsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  connectSignals();
  initDefaultStates();
}

QEventInfoPage::~QEventInfoPage() = default;

void QEventInfoPage::connectSignals() {
  // CalendarWidget -> TimelineWidget
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, mTimelineWidget,
          &QTimelineWidget::onSelectedDayChanged);
  // CalendarWidget -> EventDetailsWidget (update date)
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, this,
          &QEventInfoPage::onCalendarClicked);

  // TimelineWidget -> EventDetailsWidget (select event)
  connect(mTimelineWidget, &QTimelineWidget::eventSelected, this,
          &QEventInfoPage::onTimelineEventSelected);

  // EventDetailsWidget -> QEventInfoPage (save/cancel)
  connect(mEventDetailsWidget, &QEventDetailsWidget::provideEventSave, this,
          &QEventInfoPage::onEventSaved);
  connect(mEventDetailsWidget, &QEventDetailsWidget::provideEditingCanceled,
          this, &QEventInfoPage::onEditingCanceled);
  connect(mEventDetailsWidget, &QEventDetailsWidget::provideClientEventPairSave,
          [this](const int64_t clientId, const int64_t eventId) {
            emit provideClientEventPairSave(clientId, eventId);
          });

  connect(mEventDetailsWidget, &QEventDetailsWidget::provideFillClientComboBox,
          [this](QComboBox *comboBox) {
            emit provideFillClientComboBox(comboBox);
          });

  connect(this, &QEventInfoPage::clientResolved, this,
          &QEventInfoPage::onClientResolved);
}

void QEventInfoPage::initDefaultStates() {
  mUi->calendar_widget->setSelectedDate(QDate::currentDate());
}

void QEventInfoPage::onCalendarClicked(const QDate &date) {
  // Update the date in the details widget
  mEventDetailsWidget->findChild<QDateEdit *>("mEventDate")
      ->setDateTime(QDateTime(date, QTime::currentTime()));
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

  mEventDetailsWidget->loadEvent(event, clientId);
}

void QEventInfoPage::onEventSaved(QEventItem *event) {
  if (!event)
    return;
  qCDebug(logEventInfo) << "Event saved with ID:" << event->getId();

  auto eventDetails = event->toEvent();

  // If it's a new event, add it to the scene
  if (mEventDetailsWidget->isCreatingNewEvent()) {
    const auto id = mTimelineWidget->addEvent(eventDetails);
    eventDetails.id = id;
    // TODO: Change it later.
    event->setId(id);
  } else {
    // Otherwise, update the existing one
    mTimelineWidget->updateEvent(eventDetails);
  }

  // Request scene update
  mTimelineWidget->updateScene();
}

void QEventInfoPage::onEditingCanceled() {}
void QEventInfoPage::onClientResolved(int64_t clientId) { mClientId = clientId; }
