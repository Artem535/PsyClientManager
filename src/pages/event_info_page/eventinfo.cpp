#include "eventinfo.h"

#include "timelinewidget.h"
#include "ui/pages/ui_eventinfo.h"

#include <memory>
#include <utility>

Q_LOGGING_CATEGORY(logEventInfo, "pcm.EventInfo")

QEventInfoPage::QEventInfoPage(
    const std::shared_ptr<pcm::database::Database> &db, QWidget *parent)
    : QWidget(parent), mDB(db), mUi(std::make_unique<Ui::EventInfo>()) {
  mUi->setupUi(this);

  mTimelineWidget = new QTimelineWidget(db, this);
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

  connect(this, &QEventInfoPage::changedEditMode, [this]() {
    mUi->mEventType->setEnabled(mInEditMode);
  });
}

// Connect calendar widget signals
void QEventInfoPage::connectCalendar() {
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, mTimelineWidget,
          &QTimelineWidget::onSelectedDayChanged);
  // Update date in event info part when date is changed
  connect(mUi->calendar_widget, &QCalendarWidget::clicked,
          [&](const QDate &date) {
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

    const auto crtDateTime = QDateTime::currentDateTime().toLocalTime();
    mCurrentEvent = new QEventItem(0, "New Event", crtDateTime, crtDateTime);
    emit onEventClicked(mCurrentEvent);
    emit changedEditMode();
  });
}

// Connect Cancel/Apply buttons and mode change signals
void QEventInfoPage::connectButtonBox() {
  const auto cancelButton =
      mUi->mButtonBox->button(QDialogButtonBox::StandardButton::Cancel);
  const auto applyButton =
      mUi->mButtonBox->button(QDialogButtonBox::StandardButton::Apply);

  connect(cancelButton, &QPushButton::clicked, [this]() {
    mInEditMode = false;

    if (mCreatedNewEvent && mCurrentEvent != nullptr) {
      delete mCurrentEvent;
      mCurrentEvent = nullptr;
    }

    mCreatedNewEvent = false;
    emit changedEditMode();
  });

  // TODO: Move to separated function.
  connect(applyButton, &QPushButton::clicked, [this]() {
    mInEditMode = false;
    mCreatedNewEvent = false;
    const auto isDurationZero = mUi->mTimeFrom->time() == mUi->mTimeTo->time();
    const auto isTitleEmpty = mUi->mTitle->text().isEmpty();
    const auto isEndTimeBeforeStartTime =
        mUi->mTimeTo->dateTime() < mUi->mTimeFrom->dateTime();

    if (isDurationZero) {
      QMessageBox::warning(this, "Error",
                           "Error: Event cannot be with zero duration");
    } else if (isTitleEmpty) {
      QMessageBox::warning(this, "Error", "Error: Event title cannot be empty");
    } else if (isEndTimeBeforeStartTime) {
      QMessageBox::warning(this, "Error",
                           "Error: End time cannot be before start time");
    } else {
      emit changedEditMode();
      emit needAddNewEvent(mCurrentEvent);
      emit needSceneUpdate();
    }
  });

  connect(this, &QEventInfoPage::changedEditMode, [this]() {
    mUi->mButtonBox->setVisible(mInEditMode);
    mUi->mChangeButton->setVisible(!mInEditMode);
    mUi->mAddButton->setVisible(!mInEditMode);
  });

  connect(this, &QEventInfoPage::needAddNewEvent, this,
          &QEventInfoPage::addEvent);
}

// Connect time editors to synchronize start and end times
void QEventInfoPage::connectTimeEditors() {
  connect(mUi->mTimeFrom, &QDateTimeEdit::timeChanged, [this]() {
    // Ensure the start time is not greater than the end time
    if (mUi->mTimeFrom->dateTime() > mUi->mTimeTo->dateTime()) {
      mUi->mTimeTo->setTime(mUi->mTimeFrom->time());
    }
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
  for (const auto &client : mDB->get_clients()) {
    QString clientName = "%1 %2";
    const auto title = clientName.arg(client->name, client->last_name);
    mUi->mClientComboBox->addItem(title, QVariant::fromValue(client->id));
  }
}
void QEventInfoPage::connectEventTypes() const {
  connect(mUi->mEventType, &QCheckBox::checkStateChanged,
          [this](const int state) {
            const auto isWorkItem = state == Qt::CheckState::Checked;
            const auto title = isWorkItem ? "Work item" : "Event";
            mUi->mEventType->setText(title);
            mUi->mClientComboBox->setVisible(isWorkItem);
            mUi->mClientComboxBoxLabel->setVisible(isWorkItem);
          });
}
void QEventInfoPage::initDefaultSates()  {
  // Disable visible of client combobox
  mUi->mClientComboBox->setVisible(false);
  mUi->mClientComboxBoxLabel->setVisible(false);

  // Disable the event type checkbox
  mUi->mEventType->setEnabled(false);

  emit changedEditMode();
}

// Called when an event is clicked in the timeline
void QEventInfoPage::onEventClicked(QEventItem *event) {
  if (event != nullptr) {
    qCDebug(logEventInfo) << "EventInfoPage::onEventClicked| "
                          << "Start time:" << event->getStartTime()
                          << " End time:" << event->getEndTime()
                          << " ID:" << event->getId();

    mUi->mTitle->setText(event->getTitle());
    mUi->mTimeFrom->setDateTime(event->getStartTime());
    mUi->mTimeTo->setDateTime(event->getEndTime());
    mCurrentEvent = event;
  }
}

// Clear all UI fields
void QEventInfoPage::clearUi() const {
  mUi->mTitle->clear();
  mUi->mTimeFrom->clear();
  mUi->mTimeTo->clear();
  mUi->mEventType->setCheckState(Qt::CheckState::Unchecked);
}

// Add a new event to the timeline
void QEventInfoPage::addEvent(QEventItem *event) const {
  if (event != nullptr) {
    event->setTitle(mUi->mTitle->text());

    const auto date = mUi->mEventDate->date();
    const auto startTime = mUi->mTimeFrom->time();
    const auto endTime = mUi->mTimeTo->time();
    const auto isWorkItem =
        mUi->mEventType->checkState() == Qt::CheckState::Checked;

    event->setEndTime(QDateTime(date, endTime));
    event->setStartTime(QDateTime(date, startTime));
    event->setIsWorkItem(isWorkItem);
    mTimelineWidget->addEvent(event);
  }
}

QEventInfoPage::~QEventInfoPage() = default;
