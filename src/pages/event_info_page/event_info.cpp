#include "event_info.h"
#include "ui/pages/ui_eventinfo.h"

#include <QDateEdit>

Q_LOGGING_CATEGORY(logEventInfo, "pcm.EventInfo")

QEventInfoPage::QEventInfoPage(
    const std::shared_ptr<pcm::database::Database> &db, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()), mDb(db) {
  mUi->setupUi(this);

  mTimelineWidget = new QTimelineWidget(mDb, this);
  mUi->list_view_v_layout->addWidget(mTimelineWidget);

  mEventDetailsWidget = new QEventDetailsWidget(this);
  mUi->detailsContainer->layout()->addWidget(mEventDetailsWidget);

  // TODO: Move to separate function
  {
    QHash<obx_id, QString> clientList;

    for (const auto &client : mDb->get_clients()) {
      QString title = "%1 %2";
      const auto name = QString::fromStdString(client->name);
      const auto lastName = QString::fromStdString(client->last_name);
      title = title.arg(name, lastName);
      clientList.insertOrAssign(client->id, title);
    }

    mEventDetailsWidget->setClientList(clientList);
  }
  connectSignals();
  initDefaultStates();
}

QEventInfoPage::~QEventInfoPage() = default;

void QEventInfoPage::connectSignals() {
  // CalendarWidget -> TimelineWidget
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, mTimelineWidget,
          &QTimelineWidget::onSelectedDayChanged);
  // CalendarWidget -> EventDetailsWidget (date update)
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, this,
          &QEventInfoPage::onCalendarClicked);

  // TimelineWidget -> EventDetailsWidget (выбор события)
  connect(mTimelineWidget, &QTimelineWidget::eventSelected, this,
          &QEventInfoPage::onTimelineEventSelected);

  // EventDetailsWidget -> QEventInfoPage (сохранение/отмена)
  connect(mEventDetailsWidget, &QEventDetailsWidget::provideEventSave, this,
          &QEventInfoPage::onEventSaved);
  connect(mEventDetailsWidget, &QEventDetailsWidget::provideEditingCanceled,
          this, &QEventInfoPage::onEditingCanceled);
  connect(mEventDetailsWidget, &QEventDetailsWidget::provideClientEventPairSave,
          [this](const obx_id clientId, const obx_id eventId) {
            emit provideClientEventPairSave(clientId, eventId);
          });
}

void QEventInfoPage::initDefaultStates() {
  // Можно добавить инициализацию, если нужно
}

void QEventInfoPage::onCalendarClicked(const QDate &date) {
  // Обновляем дату в виджете деталей
  mEventDetailsWidget->findChild<QDateEdit *>("mEventDate")
      ->setDateTime(QDateTime(date, QTime::currentTime()));
}

void QEventInfoPage::onTimelineEventSelected(QEventItem *event) {
  if (!event)
    return;

  qCDebug(logEventInfo) << "Selected event with ID:" << event->getId();

  std::optional<obx_id> clientId{std::nullopt};
  if (event->isWorkItem()) {
    const auto tmpClient = mDb->get_client_by_event(event->getId());
    const auto tmpClientId = tmpClient.id;
    clientId = tmpClientId;
  }

  mEventDetailsWidget->loadEvent(event, clientId);
}

void QEventInfoPage::onEventSaved(QEventItem *event) {
  if (!event)
    return;
  qCDebug(logEventInfo) << "Event saved with ID:" << event->getId();

  auto eventDetails = event->toEvent();

  // Если это новое событие, добавляем его на сцену
  if (mEventDetailsWidget->isCreatingNewEvent()) {
    const auto id = mTimelineWidget->addEvent(eventDetails);
    eventDetails.id = id;
    // TODO: Change it later.
    event->setId(id);
  } else {
    // Иначе обновляем существующее
    mTimelineWidget->updateEvent(eventDetails);
  }

  // Запрашиваем обновление сцены
  mTimelineWidget->updateScene();
}

void QEventInfoPage::onEditingCanceled() {}