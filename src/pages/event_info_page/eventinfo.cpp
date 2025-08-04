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
  // TODO: rename layout name
  mUi->list_view_v_layout->addWidget(mTimelineWidget);

  // Connect the timeline widget to the event view, for update information about
  // event.
  connect(mTimelineWidget, &TimelineWidget::eventSelected, this,
          &EventInfoPage::onEventClicked);

  // Connect the buttons.
  connect(mUi->mChangeButton, &QPushButton::clicked, [&]() {
    mInEditMode = true;
    emit changedEditMode();
  });

  // In edit we display only buttons with apply and cancel.
  connect(this, &EventInfoPage::changedEditMode, [&]() {
    mUi->mButtonBox->setVisible(mInEditMode);
    mUi->mChangeButton->setVisible(!mInEditMode);
  });

  connect(mUi->mButtonBox->button(QDialogButtonBox::StandardButton::Cancel),
          &QPushButton::clicked, [&]() {
            mInEditMode = false;
            emit changedEditMode();
          });

  connect(mUi->mButtonBox->button(QDialogButtonBox::StandardButton::Apply),
          &QPushButton::clicked, [&]() {
            mInEditMode = false;
            emit changedEditMode();
          });

  emit changedEditMode();
}

void EventInfoPage::onEventClicked(EventItem *event) {
  if (event != nullptr) {
    mUi->mTitle->setText(event->getTitle());
    mUi->mTimeFrom->setDateTime(event->getStartTime());
    mUi->mTimeTo->setDateTime(event->getEndTime());
  }
}

EventInfoPage::~EventInfoPage() = default;