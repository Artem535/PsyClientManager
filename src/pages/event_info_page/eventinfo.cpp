#include "eventinfo.h"

#include "timelinewidget.h"
#include "ui/pages/ui_eventinfo.h"

#include <memory>
#include <utility>

Q_LOGGING_CATEGORY(logEventInfo, "pcm.EventInfo")

QEventInfoPage::QEventInfoPage(
    const std::shared_ptr<pcm::database::Database> &db, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()) {
  mUi->setupUi(this);

  mTimelineWidget = new QTimelineWidget(db, this);
  mUi->list_view_v_layout->addWidget(mTimelineWidget);

  connectCalendar();
  connectTimeline();
  connectButtons();
  connectButtonBox();
  connectTimeEditors();
  connectSceneUpdate();

  initDefaultTimes();

  emit changedEditMode();
}

// Connect calendar widget signals
void QEventInfoPage::connectCalendar() {
  connect(mUi->calendar_widget, &QCalendarWidget::clicked, mTimelineWidget,
          &QTimelineWidget::onSelectedDayChanged);
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
    mCurrentEvent = new QEventItem(-1, "New Event", crtDateTime, crtDateTime);
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

  connect(applyButton, &QPushButton::clicked, [this]() {
    mInEditMode = false;
    mCreatedNewEvent = false;
    if (mUi->mTimeFrom->time() != mUi->mTimeTo->time()) {
      emit changedEditMode();
      emit needAddNewEvent(mCurrentEvent);
      emit needSceneUpdate();
    } else {
      QMessageBox::warning(this, "Error",
                           "Error: Event cannot be with zero duration");
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
  mUi->mTimeFrom->setDateTime(crtDateTime);
  mUi->mTimeTo->setDateTime(crtDateTime);
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
    event->setStartTime(mUi->mTimeFrom->dateTime());
    event->setEndTime(mUi->mTimeTo->dateTime());
    event->setIsWorkItem(mUi->mEventType->checkState() ==
                         Qt::CheckState::Checked);
    mTimelineWidget->addEvent(event);
  }
}

QEventInfoPage::~QEventInfoPage() = default;
