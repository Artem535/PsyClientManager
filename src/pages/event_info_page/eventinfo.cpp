#include "eventinfo.h"
#include "eventitem.h"
#include "eventview.h"
#include "timelinewidget.h"
#include "ui/pages/ui_eventinfo.h"
#include <memory>
#include <qdialogbuttonbox.h>
#include <qpushbutton.h>

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
  }
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
}

void EventInfoPage::connectButtonBox() {
  auto cancelButton = mUi->mButtonBox->button(QDialogButtonBox::StandardButton::Cancel);
  auto applyButton = mUi->mButtonBox->button(QDialogButtonBox::StandardButton::Apply);

  connect(cancelButton, &QPushButton::clicked, [this]() {
    mInEditMode = false;
    emit changedEditMode();
  });

  connect(applyButton, &QPushButton::clicked, [this]() {
    mInEditMode = false;
    emit changedEditMode();
  });

  // Подключаем изменение режима для управления видимостью элементов
  connect(this, &EventInfoPage::changedEditMode, [this]() {
    mUi->mButtonBox->setVisible(mInEditMode);
    mUi->mChangeButton->setVisible(!mInEditMode);
  });
}

EventInfoPage::~EventInfoPage() = default;