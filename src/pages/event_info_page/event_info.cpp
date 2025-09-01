#include "event_info.h"
#include "ui/pages/ui_eventinfo.h"



Q_LOGGING_CATEGORY(logEventInfo, "pcm.EventInfo")

QEventInfoPage::QEventInfoPage(
    const std::shared_ptr<pcm::database::Database> &db, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>().release()),
      mDb(db) {
  mUi->setupUi(this);

  mTimelineWidget = new QTimelineWidget(mDb, this);
  mUi->list_view_v_layout->addWidget(mTimelineWidget);

  connectCalendar();
  connectTimeline();
  connectButtons();
  connectButtonBox();
  connectTimeEditors();
  connectSceneUpdate();
  connectEventTypes();

  initDefaultTimes();
  initClientComboBox();
  initDefaultSates();

  connect(this, &QEventInfoPage::changedEditMode,
          [this]() { mUi->mEventType->setEnabled(mInEditMode); });

  updateButtonState();
}

// Connect calendar widget signals
void QEventInfoPage::connectCalendar() {
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, mTimelineWidget,
          &QTimelineWidget::onSelectedDayChanged);
  // Update date in event info part when date is changed
  connect(mUi->calendar_widget, &QCalendarWidget::clicked,
          [this](const QDate &date) {
            mUi->mEventDate->setDateTime(QDateTime(date, QTime::currentTime()));
          });
}

// Connect timeline widget signals
void QEventInfoPage::connectTimeline() {
  connect(mTimelineWidget, &QTimelineWidget::eventSelected, this,
          &QEventInfoPage::onEventClicked);
}

// Connect Add and Change buttons
void QEventInfoPage::connectButtons() {
  // Button for change
  connect(mUi->mChangeButton, &QPushButton::clicked, [this]() {
    mInEditMode = true;
    emit changedEditMode();
  });

  connect(mUi->mAddButton, &QPushButton::clicked, [this]() {
    mInEditMode = true;
    mCreatedNewEvent = true;

    const auto crtDateTime = QDateTime::currentDateTime().toLocalTime();
    mCurrentEvent = new QEventItem(0, "New Event", crtDateTime, crtDateTime);
    emit onEventClicked(mCurrentEvent);
    emit changedEditMode();
  });
}

// Connect Cancel/Apply buttons and mode change signals
void QEventInfoPage::connectButtonBox() {
  const auto cancelButton = mUi->mButtonBox->button(QDialogButtonBox::Cancel);
  const auto applyButton = mUi->mButtonBox->button(QDialogButtonBox::Apply);

  connect(cancelButton, &QPushButton::clicked, [this]() {
    mInEditMode = false;

    if (mCreatedNewEvent && mCurrentEvent) {
      delete mCurrentEvent.data();
      mCurrentEvent.clear();
    }

    mCreatedNewEvent = false;
    emit changedEditMode();
  });

  connect(applyButton, &QPushButton::clicked, [this]() {
    if (!mCurrentEvent || !validateInput()) {
      return;
    }

    mInEditMode = false;
    mCreatedNewEvent = false;

    emit changedEditMode();
    emit needAddNewEvent(mCurrentEvent.data());
    emit needSceneUpdate();
  });

  connect(this, &QEventInfoPage::changedEditMode, [this]() {
    const bool inEdit = mInEditMode;
    mUi->mButtonBox->setVisible(inEdit);
    mUi->mChangeButton->setVisible(!inEdit);
    mUi->mAddButton->setVisible(!inEdit);
  });

  connect(this, &QEventInfoPage::needAddNewEvent, this,
          &QEventInfoPage::addEvent);
}

// Синхронизация времени: start не может быть больше end
void QEventInfoPage::connectTimeEditors() {
  connect(mUi->mTimeFrom, &QDateTimeEdit::dateTimeChanged,
          [this](const QDateTime &dt) {
            if (mUi->mTimeTo->dateTime() < dt) {
              mUi->mTimeTo->setDateTime(dt.addSecs(60)); // Минимум 1 минута
            }
            updateButtonState();
          });

  connect(mUi->mTimeTo, &QDateTimeEdit::dateTimeChanged,
          [this](const QDateTime &dt) {
            if (dt < mUi->mTimeFrom->dateTime()) {
              mUi->mTimeFrom->setDateTime(dt.addSecs(-60));
            }
            updateButtonState();
          });
}

void QEventInfoPage::connectSceneUpdate() {
  connect(this, &QEventInfoPage::needSceneUpdate, mTimelineWidget,
          &QTimelineWidget::updateScene);
}

// Initialize default start/end times to current time
void QEventInfoPage::initDefaultTimes() const {
  const auto crtDateTime = QDateTime::currentDateTime();
  mUi->mEventDate->setDateTime(crtDateTime);
  mUi->mTimeFrom->setDateTime(crtDateTime);
  mUi->mTimeTo->setDateTime(crtDateTime);
}

void QEventInfoPage::initClientComboBox() const {
  mUi->mClientComboBox->clear();
  for (const auto &client : mDb->get_clients()) {
    QString clientName = "%1 %2";
    const auto title =
        clientName.arg(QString::fromStdString(client->name),
                       QString::fromStdString(client->last_name));
    mUi->mClientComboBox->addItem(title, QVariant::fromValue(client->id));
  }
}

void QEventInfoPage::connectEventTypes() const {
  connect(mUi->mEventType, &QCheckBox::toggled, [this](const bool checked) {
    mUi->mEventType->setText(checked ? "Work item" : "Event");
    mUi->mClientComboBox->setVisible(checked);
    mUi->mClientComboxBoxLabel->setVisible(checked);
  });
}

void QEventInfoPage::initDefaultSates() {
  mUi->mClientComboBox->setVisible(false);
  mUi->mClientComboxBoxLabel->setVisible(false);
  mUi->mEventType->setEnabled(false);
  emit changedEditMode();
}

// Called when an event is clicked in the timeline
void QEventInfoPage::onEventClicked(QEventItem *event) {
  if (!event)
    return;

  qCDebug(logEventInfo) << "EventInfoPage::onEventClicked|"
                        << "Start time:" << event->getStartTime()
                        << "End time:" << event->getEndTime()
                        << "ID:" << event->getId();

  mUi->mTitle->setText(event->getTitle());
  mUi->mTimeFrom->setDateTime(event->getStartTime());
  mUi->mTimeTo->setDateTime(event->getEndTime());

  const bool isWorkItem = event->isWorkItem();
  mUi->mEventType->setChecked(isWorkItem);
  mCurrentEvent = event;

  if (isWorkItem) {
    const auto client = mDb->get_client_by_event(event->getId());
    const auto clientId = QVariant::fromValue(client.id);
    if (const int index = mUi->mClientComboBox->findData(clientId);
        index != -1) {
      mUi->mClientComboBox->setCurrentIndex(index);
    }
  }

  updateButtonState();
}

// Clear all UI fields
void QEventInfoPage::clearUi() const {
  mUi->mTitle->clear();
  mUi->mTimeFrom->clear();
  mUi->mTimeTo->clear();
  mUi->mEventType->setChecked(false);
}

// Add a new event to the timeline
void QEventInfoPage::addEvent(QEventItem *event) const {
  if (!event)
    return;

  event->setTitle(mUi->mTitle->text());
  const auto date = mUi->mEventDate->date();
  const auto startTime = QDateTime(date, mUi->mTimeFrom->time());
  const auto endTime = QDateTime(date, mUi->mTimeTo->time());
  const bool isWorkItem = mUi->mEventType->isChecked();

  event->setEndTime(endTime);
  event->setStartTime(startTime);
  event->setIsWorkItem(isWorkItem);

  const auto eventId = mTimelineWidget->addEvent(event);

  if (isWorkItem) {
    const auto clientId = mUi->mClientComboBox->currentData().toULongLong();
    [[maybe_unused]] const auto dbId = mDb->add_event_client(eventId, clientId);
  }
}

// Centralized validation with user feedback
bool QEventInfoPage::validateInput() {
  const bool isTitleEmpty = mUi->mTitle->text().trimmed().isEmpty();
  const bool isZeroDuration =
      mUi->mTimeTo->dateTime() <= mUi->mTimeFrom->dateTime();

  if (isTitleEmpty) {
    QMessageBox::warning(this, tr("Error"), tr("Event title cannot be empty"));
    return false;
  }
  if (isZeroDuration) {
    QMessageBox::warning(this, tr("Error"),
                         tr("Event duration must be greater than zero"));
    return false;
  }
  return true;
}

// Update Apply button state reactively
void QEventInfoPage::updateButtonState() const {
  const bool isValid = !mUi->mTitle->text().trimmed().isEmpty() &&
                       mUi->mTimeTo->dateTime() > mUi->mTimeFrom->dateTime();

  if (const auto applyButton =
          mUi->mButtonBox->button(QDialogButtonBox::Apply)) {
    applyButton->setEnabled(isValid);
  }
}

QEventInfoPage::~QEventInfoPage() = default;