#include "eventinfo.h"
#include "ui/pages/ui_eventinfo.h" // <-- UI-заголовок теперь здесь
#include <memory>

EventInfoPage::EventInfoPage(std::shared_ptr<pcm::database::Database> db,
                             QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()) {
  mUi->setupUi(this);

  mTimelineWidget = new TimelineWidget(db, this);
  mUi->list_view_v_layout->addWidget(mTimelineWidget);

  connectCalendar();
  connectTimeline();
  connectButtons();
  connectButtonBox();

  emit changedEditMode(); // Инициализация видимости кнопок
}

void EventInfoPage::onEventClicked(EventItem *event) {
  if (event != nullptr) {
    mUi->mTitle->setText(event->getTitle());
    mUi->mTimeFrom->setDateTime(event->getStartTime());
    mUi->mTimeTo->setDateTime(event->getEndTime());
    mCurrentEvent = event;
  }
}

void EventInfoPage::clearUi() {
  mUi->mTitle->clear();
  mUi->mTimeFrom->clear();
  mUi->mTimeTo->clear();
  mUi->mEventType->setCheckState(Qt::CheckState::Unchecked);
}

void EventInfoPage::connectCalendar() {
  connect(mUi->calendar_widget, &QCalendarWidget::selectionChanged, [this]() {
    const auto selectedDate = mUi->calendar_widget->selectedDate();
    mTimelineWidget->onSelectedDayChanged(selectedDate);
  });
}

void EventInfoPage::connectTimeline() {
  connect(mTimelineWidget, &TimelineWidget::eventSelected, this,
          &EventInfoPage::onEventClicked);
}

void EventInfoPage::connectButtons() {
  connect(mUi->mChangeButton, &QPushButton::clicked, [this]() {
    mInEditMode = true;
    emit changedEditMode();
  });

  connect(mUi->mAddButton, &QPushButton::clicked, [this]() {
    mInEditMode = true;

    const auto crtDateTime = QDateTime::currentDateTime();
    mCurrentEvent = new EventItem(-1, "", crtDateTime, crtDateTime);
    emit onEventClicked(mCurrentEvent);
    emit changedEditMode();
  });
}

void EventInfoPage::connectButtonBox() {
  auto cancelButton =
      mUi->mButtonBox->button(QDialogButtonBox::StandardButton::Cancel);
  auto applyButton =
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
    emit changedEditMode();
    emit needAddNewEvent(mCurrentEvent);
  });

  // Подключаем изменение режима для управления видимостью элементов
  connect(this, &EventInfoPage::changedEditMode, [this]() {
    mUi->mButtonBox->setVisible(mInEditMode);
    mUi->mChangeButton->setVisible(!mInEditMode);
    mUi->mAddButton->setVisible(!mInEditMode);
  });

  connect(this, &EventInfoPage::needAddNewEvent, this,
          &EventInfoPage::addEvent);
}

void EventInfoPage::addEvent(EventItem *event) {
  if (event != nullptr) {
    event->setTitle(mUi->mTitle->text());
    event->setStartTime(mUi->mTimeFrom->dateTime());
    event->setEndTime(mUi->mTimeTo->dateTime());
    event->setIsWorkItem(mUi->mEventType->checkState() ==
                         Qt::CheckState::Checked);
    mTimelineWidget->addEvent(event);
  }
}

EventInfoPage::~EventInfoPage() = default;