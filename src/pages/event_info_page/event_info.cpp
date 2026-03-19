#include "event_info.h"
#include "../../widgets/constants.hpp"
#include "../../widgets/app_settings.h"
#include "ui/pages/ui_eventinfo.h"

#include <QDialog>
#include <QIcon>
#include <QLayout>
#include <QMessageBox>
#include <QSize>
#include <QTextCharFormat>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(logEventInfo, "pcm.EventInfo")

QEventInfoPage::QEventInfoPage(QTimelineModel *model, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()) {
  mUi->setupUi(this);
  mUi->list_view_layout->setColumnStretch(0, 1);
  mUi->list_view_layout->setColumnStretch(1, 0);

  mSelectedDate = QDate::currentDate();
  mCalendarWidget = new RoundedCalendarWidget(this);
  mCalendarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  mUi->calendarCardLayout->replaceWidget(mUi->calendar_widget, mCalendarWidget);
  mUi->calendar_widget->hide();
  mUi->calendar_widget->deleteLater();

  mTimelineWidget = new QTimelineWidget(model, this);
  mUi->list_view_layout->addWidget(mTimelineWidget, 0, 0, 2, 1);

  mCreateEventButton = new QPushButton(this);
  mCreateEventButton->setIcon(QIcon(":/icons/calendar-plus-solid-full.svg"));
  mCreateEventButton->setToolTip(tr(": EVENT_ADD_BUTTON"));
  mCreateEventButton->setIconSize(QSize(18, 18));
  mCreateEventButton->setFixedSize(40, 40);
  mCreateEventButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  mUi->list_view_layout->addWidget(mCreateEventButton, 0, 1,
                                   Qt::AlignTop | Qt::AlignRight);
  mUi->frame_2->setVisible(false);

  connectSignals();
  initDefaultStates();
}

QEventInfoPage::~QEventInfoPage() = default;

void QEventInfoPage::connectSignals() {
  // CalendarWidget -> TimelineWidget
  connect(mCalendarWidget, &RoundedCalendarWidget::clicked, mTimelineWidget,
          &QTimelineWidget::onSelectedDayChanged);
  connect(mCalendarWidget, &RoundedCalendarWidget::clicked, this,
          &QEventInfoPage::onCalendarClicked);
  connect(mCreateEventButton, &QPushButton::clicked, this,
          &QEventInfoPage::onCreateEventClicked);

  connect(mTimelineWidget, &QTimelineWidget::eventSelected, this,
          &QEventInfoPage::onTimelineEventSelected);
  connect(mTimelineWidget, &QTimelineWidget::eventEditRequested, this,
          &QEventInfoPage::onTimelineEventEditRequested);
  connect(mTimelineWidget, &QTimelineWidget::eventDeleteRequested, this,
          &QEventInfoPage::onTimelineEventDeleteRequested);

  connect(this, &QEventInfoPage::clientResolved, this,
          &QEventInfoPage::onClientResolved);
}

void QEventInfoPage::initDefaultStates() {
  mSelectedDate = QDate::currentDate();
  mCalendarWidget->setSelectedDate(mSelectedDate);
  updateCalendarHighlights();
  mTimelineWidget->onSelectedDayChanged(mSelectedDate);
}

void QEventInfoPage::onCalendarClicked(const QDate &date) {
  mSelectedDate = date;
}

void QEventInfoPage::updateCalendarHighlights() const {
  QTextCharFormat currentDayFormat;
  currentDayFormat.setFontWeight(QFont::DemiBold);
  currentDayFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);
  currentDayFormat.setUnderlineColor(
      pcm::widgets::constants::kCalendarCurrentDayUnderlineColor);
  currentDayFormat.setForeground(
      pcm::widgets::constants::kCalendarCurrentDayForegroundColor);

  mCalendarWidget->setDateTextFormat(QDate::currentDate(), currentDayFormat);
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
  detailsWidget->setConflictChecker([this](const DuckEvent &event) {
    return pcm::app_settings::preventEventOverlaps() && mTimelineWidget &&
           mTimelineWidget->hasConflict(event);
  });
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
  editEventWithDialog(event);
}

void QEventInfoPage::onTimelineEventEditRequested(QEventItem *event) {
  editEventWithDialog(event);
}

void QEventInfoPage::onTimelineEventDeleteRequested(const int64_t eventId) {
  if (pcm::app_settings::confirmEventDeletion()) {
    const auto reply =
        QMessageBox::question(this, tr(": EVENT_DELETE_TITLE"),
                              tr(": EVENT_DELETE_CONFIRMATION"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) {
      return;
    }
  }

  mTimelineWidget->removeEvent(eventId);
  mTimelineWidget->onSelectedDayChanged(mSelectedDate);
}

void QEventInfoPage::editEventWithDialog(QEventItem *event) {
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
  const auto selectedClientId =
      mActiveEventDetailsWidget ? mActiveEventDetailsWidget->selectedClientId() : 0;
  const auto selectedClientName =
      mActiveEventDetailsWidget ? mActiveEventDetailsWidget->selectedClientName() : QString{};

  if (mActiveEventDetailsWidget && mActiveEventDetailsWidget->isCreatingNewEvent()) {
    const auto id = mTimelineWidget->addEvent(
        eventDetails, !pcm::app_settings::preventEventOverlaps());
    if (id <= 0) {
      qCWarning(logEventInfo) << "Failed to persist event in DB";
      return;
    }
    eventDetails.id = id;
    event->setId(id);
  } else {
    mTimelineWidget->updateEvent(
        eventDetails, !pcm::app_settings::preventEventOverlaps());
  }

  if (eventDetails.is_work_event && selectedClientId > 0 && eventDetails.id > 0) {
    emit provideClientEventPairSave(selectedClientId, eventDetails.id);
    event->setClientName(selectedClientName);
  }

  // Force reload from DB to avoid stale UI state.
  mTimelineWidget->onSelectedDayChanged(mSelectedDate);
}

void QEventInfoPage::onEditingCanceled() {}
void QEventInfoPage::onClientResolved(int64_t clientId) { mClientId = clientId; }

void QEventInfoPage::refreshAppearance() {
  if (!mTimelineWidget) {
    return;
  }

  mTimelineWidget->updateScene();
  mTimelineWidget->update();
}
