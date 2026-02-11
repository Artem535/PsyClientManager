#include "qevent_details_widget.h"
#include "ui/pages/ui_eventdetails.h"

#include <QMessageBox>

Q_LOGGING_CATEGORY(logEventDetails, "pcm.EventDetails")

QEventDetailsWidget::QEventDetailsWidget(QWidget *parent)
    : QWidget(parent), mUI(std::make_unique<Ui::EventDetails>()) {
  mUI->setupUi(this);
  initUi();
  initConnections();
  initDefaultStyle();
  initDefaultStates();
  initDefaultTimes();
}

QEventDetailsWidget::~QEventDetailsWidget() = default;


void QEventDetailsWidget::initUi() {}

void QEventDetailsWidget::initConnections() {
  // --- Button Connections ---
  connect(mUI->mButtonBox->button(QDialogButtonBox::Apply),
          &QPushButton::clicked, this, &QEventDetailsWidget::onApplyClicked);
  connect(mUI->mButtonBox->button(QDialogButtonBox::Cancel),
          &QPushButton::clicked, this, &QEventDetailsWidget::onCancelClicked);
  connect(mUI->mAddButton, &QPushButton::clicked, this,
          &QEventDetailsWidget::onAddClicked);
  connect(mUI->mChangeButton, &QPushButton::clicked, this,
          &QEventDetailsWidget::onChangeClicked);

  // --- Input Change Connections ---
  connect(mUI->mEventType, &QCheckBox::toggled, this,
          &QEventDetailsWidget::onEventTypeToggled);
  connect(mUI->mTimeFrom, &QDateTimeEdit::dateTimeChanged, this,
          &QEventDetailsWidget::onTimeFromChanged);
  connect(mUI->mTimeTo, &QDateTimeEdit::dateTimeChanged, this,
          &QEventDetailsWidget::onTimeToChanged);
}

void QEventDetailsWidget::initDefaultStyle() {
  if (mDialogMode) {
    initEditStyle();
    return;
  }

  const auto isVisible = mCurrentEvent ? mCurrentEvent->isWorkItem() : false;
  mUI->mClientComboBox->setVisible(isVisible);
  mUI->mClientComboxBoxLabel->setVisible(isVisible);
  mUI->mEventType->setEnabled(false);
  mUI->mButtonBox->setVisible(false);
  mUI->mChangeButton->setVisible(true);
  mUI->mAddButton->setVisible(true);
  emit provideFillClientComboBox(mUI->mClientComboBox);
}

void QEventDetailsWidget::initEditStyle() {
  mUI->mButtonBox->setVisible(true);
  mUI->mChangeButton->setVisible(false);
  mUI->mAddButton->setVisible(false);
  mUI->mEventType->setEnabled(true);
  emit provideFillClientComboBox(mUI->mClientComboBox);
}

void QEventDetailsWidget::initDefaultStates() const {
  // Set initial text for checkbox
  mUI->mEventType->setText(tr(": EVENT_TYPE_REGULAR"));
}

void QEventDetailsWidget::initDefaultTimes() const {
  const auto crtDateTime = QDateTime::currentDateTime();
  mUI->mEventDate->setDateTime(crtDateTime);
  mUI->mTimeFrom->setDateTime(crtDateTime);
  mUI->mTimeTo->setDateTime(crtDateTime.addSecs(3600)); // +1 hour
}

void QEventDetailsWidget::setClientList(const QHash<int64_t, QString> &clients) {
  mClientList = clients;
  mUI->mClientComboBox->clear();
  for (auto it = mClientList.constBegin(); it != mClientList.constEnd(); ++it) {
    mUI->mClientComboBox->addItem(it.value(), QVariant::fromValue(it.key()));
  }
}

void QEventDetailsWidget::loadEvent(QEventItem *event,
                                    const std::optional<int64_t> clientId) {
  if (!event)
    return;

  qCDebug(logEventDetails) << "EventDetailsWidget::loadEvent|"
                           << "Start time:" << event->getStartTime()
                           << "End time:" << event->getEndTime()
                           << "ID:" << event->getId();

  mCurrentEvent = event;
  mUI->mTitle->setText(event->getTitle());
  mUI->mEventDate->setDate(event->getStartTime().date());
  mUI->mTimeFrom->setDateTime(event->getStartTime());
  mUI->mTimeTo->setDateTime(event->getEndTime());
  const bool isWorkItem = event->isWorkItem();
  mUI->mEventType->setChecked(isWorkItem);

  if (isWorkItem && event->getId() != 0) {
    // Find selected client ID in the client list
    if (clientId) {
      // TODO: Find a way to avoid this
      emit provideFillClientComboBox(mUI->mClientComboBox);
      const auto varClientId = QVariant::fromValue(clientId.value());
      if (const int index = mUI->mClientComboBox->findData(varClientId);
          index != -1) {
        mUI->mClientComboBox->setCurrentIndex(index);
      }
    }
  }

  updateButtonState();
}

void QEventDetailsWidget::startEditingEvent(
    QEventItem *event, const std::optional<int64_t> clientId) {
  if (!event) {
    return;
  }

  mCreatingNewEvent = false;
  mInEditMode = true;
  emit provideEditModeChanged();

  loadEvent(event, clientId);
  initEditStyle();
}

void QEventDetailsWidget::startCreatingNewEvent(const QDate &date) {
  mCreatingNewEvent = true;
  mInEditMode = true;
  emit provideEditModeChanged();

  const auto crtDateTime = QDateTime::currentDateTime().toLocalTime();
  // Create a temporary event for the form
  mCurrentEvent =
      new QEventItem(0, tr(": EVENT_NEW_TITLE"), crtDateTime,
                     crtDateTime.addSecs(3600));
  loadEvent(mCurrentEvent.data());
  mUI->mEventDate->setDate(date);
  initEditStyle();
}

void QEventDetailsWidget::setDialogMode(bool enabled) {
  mDialogMode = enabled;
  initDefaultStyle();
}

bool QEventDetailsWidget::isInEditMode() const { return mInEditMode; }

bool QEventDetailsWidget::isCreatingNewEvent() const {
  return mCreatingNewEvent;
}

QEventItem *QEventDetailsWidget::currentEvent() const {
  return mCurrentEvent.data();
}

void QEventDetailsWidget::onApplyClicked() {
  if (!validateInput()) {
    return;
  }

  if (mCurrentEvent) {
    mCurrentEvent->setTitle(mUI->mTitle->text());
    mCurrentEvent->setStartTime(
        QDateTime(mUI->mEventDate->date(), mUI->mTimeFrom->time()));
    mCurrentEvent->setEndTime(
        QDateTime(mUI->mEventDate->date(), mUI->mTimeTo->time()));
    mCurrentEvent->setIsWorkItem(mUI->mEventType->isChecked());
  }

  const bool isCreatingNewEvent = mCreatingNewEvent;

  // Emit signal to save the event
  emit provideEventSave(mCurrentEvent.data());

  if (mCurrentEvent) {
    int64_t selectedClientId = 0;
    if (mUI->mEventType->isChecked()) {
      selectedClientId = mUI->mClientComboBox->currentData().toLongLong();
      qCDebug(logEventDetails)
          << "Selected client ID for event:" << selectedClientId;
    }
    emit provideClientEventPairSave(selectedClientId, mCurrentEvent->getId());
  }

  if (isCreatingNewEvent && mCurrentEvent) {
    delete mCurrentEvent.data();
    mCurrentEvent.clear();
  }

  // Exit edit mode
  mInEditMode = false;
  mCreatingNewEvent = false;
  emit provideEditModeChanged();
  initDefaultStyle();
}

void QEventDetailsWidget::onCancelClicked() {
  mInEditMode = false;
  if (mCreatingNewEvent && mCurrentEvent) {
    delete mCurrentEvent.data();
    mCurrentEvent.clear();
  }
  mCreatingNewEvent = false;
  emit provideEditModeChanged();
  emit provideEditingCanceled();
  initDefaultStyle();
}

void QEventDetailsWidget::onAddClicked() { startCreatingNewEvent(); }

void QEventDetailsWidget::onChangeClicked() {
  if (!mCurrentEvent)
    return;
  mInEditMode = true;
  emit provideEditModeChanged();
  initEditStyle();
}

void QEventDetailsWidget::onEventTypeToggled(bool checked) {
  mUI->mEventType->setText(
      checked ? tr(": EVENT_TYPE_WORK") : tr(": EVENT_TYPE_REGULAR"));
  mUI->mClientComboBox->setVisible(checked);
  mUI->mClientComboxBoxLabel->setVisible(checked);
}

void QEventDetailsWidget::onTimeFromChanged(const QDateTime &dt) {
  if (mUI->mTimeTo->dateTime() < dt) {
    mUI->mTimeTo->setDateTime(dt.addSecs(60)); // At least 1 minute
  }
  updateButtonState();
}

void QEventDetailsWidget::onTimeToChanged(const QDateTime &dt) {
  if (dt < mUI->mTimeFrom->dateTime()) {
    mUI->mTimeFrom->setDateTime(dt.addSecs(-60));
  }
  updateButtonState();
}

void QEventDetailsWidget::updateButtonState() const {
  const bool isValid = !mUI->mTitle->text().trimmed().isEmpty() &&
                       mUI->mTimeTo->dateTime() > mUI->mTimeFrom->dateTime();
  if (auto *applyButton = mUI->mButtonBox->button(QDialogButtonBox::Apply)) {
    applyButton->setEnabled(isValid);
  }
}

bool QEventDetailsWidget::validateInput() {
  const bool isTitleEmpty = mUI->mTitle->text().trimmed().isEmpty();
  const bool isZeroDuration =
      mUI->mTimeTo->dateTime() <= mUI->mTimeFrom->dateTime();

  if (isTitleEmpty) {
    QMessageBox::warning(this, tr(": ERROR_TITLE"),
                         tr(": EVENT_TITLE_EMPTY_ERROR"));
    return false;
  }
  if (isZeroDuration) {
    QMessageBox::warning(this, tr(": ERROR_TITLE"),
                         tr(": EVENT_DURATION_INVALID_ERROR"));
    return false;
  }
  return true;
}

ObxEvent QEventDetailsWidget::collectEventData() const {
  // This method is no longer used in the main logic flow.
  // It is left for backward compatibility or internal use if needed.
  ObxEvent event;
  event.name = mUI->mTitle->text().toStdString();
  event.start_date = QDateTime(mUI->mEventDate->date(), mUI->mTimeFrom->time())
                         .toMSecsSinceEpoch();
  event.end_date = QDateTime(mUI->mEventDate->date(), mUI->mTimeTo->time())
                       .toMSecsSinceEpoch();
  event.is_work_event = mUI->mEventType->isChecked();
  event.duration = (event.end_date.value_or(0) - event.start_date.value_or(0)) / 1000; // in seconds
  return event;
}
