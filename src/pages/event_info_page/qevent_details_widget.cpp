#include "qevent_details_widget.h"
#include "../../widgets/app_settings.h"
#include "../../widgets/meeting_utils.h"
#include "ui/pages/ui_eventdetails.h"

#include <oclero/qlementine/widgets/Switch.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QTimeZone>

Q_LOGGING_CATEGORY(logEventDetails, "pcm.EventDetails")

namespace {
constexpr int64_t kPaymentPendingId = 1;
constexpr int64_t kPaymentPaidId = 2;
constexpr int64_t kPaymentCanceledId = 3;
constexpr int64_t kPaymentRefundedId = 4;
constexpr int64_t kPaymentSkippedId = 5;
}

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


void QEventDetailsWidget::initUi() {
  const auto localTz = QTimeZone::systemTimeZone();
  mUI->mEventDate->setTimeZone(localTz);
  mUI->mTimeFrom->setTimeZone(localTz);
  mUI->mTimeTo->setTimeZone(localTz);
  mUI->mTimeFrom->setDisplayFormat("HH:mm");
  mUI->mTimeTo->setDisplayFormat("HH:mm");
  mUI->mCostSpinBox->setDecimals(2);
  mUI->mCostSpinBox->setMinimum(0.0);
  mUI->mCostSpinBox->setMaximum(1'000'000.0);
  mUI->mCostSpinBox->setSingleStep(100.0);
  mUI->mCostSpinBox->setSuffix(tr(" ₽"));
  mUI->mPaymentStatusComboBox->addItem(tr("Pending"),
                                       QVariant::fromValue(kPaymentPendingId));
  mUI->mPaymentStatusComboBox->addItem(tr("Paid"),
                                       QVariant::fromValue(kPaymentPaidId));
  mUI->mPaymentStatusComboBox->addItem(tr("Canceled"),
                                       QVariant::fromValue(kPaymentCanceledId));
  mUI->mPaymentStatusComboBox->addItem(tr("Refunded"),
                                       QVariant::fromValue(kPaymentRefundedId));
  mUI->mPaymentStatusComboBox->addItem(tr("Skipped"),
                                       QVariant::fromValue(kPaymentSkippedId));

  mEventTypeSwitch = new oclero::qlementine::Switch(this);
  mEventTypeSwitch->setText(mUI->mEventType->text());
  mUI->formLayout->replaceWidget(mUI->mEventType, mEventTypeSwitch);
  mUI->mEventType->hide();
  mUI->mEventType->deleteLater();

  mOnlineSessionSwitch = new oclero::qlementine::Switch(this);
  mOnlineSessionSwitch->setText(tr("Online session"));
  constexpr int onlineSectionRow = 7;
  mUI->formLayout->insertRow(onlineSectionRow, tr("Session format"),
                             mOnlineSessionSwitch);

  mMeetingUrlLabel = new QLabel(tr("Meeting link"), this);
  mMeetingUrlEdit = new oclero::qlementine::LineEdit(this);
  mMeetingUrlEdit->setPlaceholderText(tr("https://..."));
  mMeetingUrlEdit->setIcon(QIcon(":/icons/calendar-solid-full.svg"));
  mUI->formLayout->insertRow(onlineSectionRow + 1, mMeetingUrlLabel,
                             mMeetingUrlEdit);

  mMeetingActionsWidget = new QWidget(this);
  auto *meetingActionsLayout = new QHBoxLayout(mMeetingActionsWidget);
  meetingActionsLayout->setContentsMargins(0, 0, 0, 0);
  meetingActionsLayout->setSpacing(8);
  mOpenMeetingButton = new QPushButton(tr("Open"), mMeetingActionsWidget);
  mCopyMeetingUrlButton = new QPushButton(tr("Copy link"), mMeetingActionsWidget);
  mCopyMeetingInviteButton = new QPushButton(tr("Copy invite"), mMeetingActionsWidget);
  meetingActionsLayout->addWidget(mOpenMeetingButton);
  meetingActionsLayout->addWidget(mCopyMeetingUrlButton);
  meetingActionsLayout->addWidget(mCopyMeetingInviteButton);
  meetingActionsLayout->addStretch();
  mUI->formLayout->insertRow(onlineSectionRow + 2, QString(),
                             mMeetingActionsWidget);

  mUI->mAddButton->setIcon(QIcon(":/icons/calendar-plus-solid-full.svg"));
  mUI->mAddButton->setIconSize(QSize(16, 16));
  mUI->mChangeButton->setIcon(QIcon(":/icons/user-pen-solid-full.svg"));
  mUI->mChangeButton->setIconSize(QSize(16, 16));
}

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
  connect(mEventTypeSwitch, &QAbstractButton::toggled, this,
          &QEventDetailsWidget::onEventTypeToggled);
  connect(mOnlineSessionSwitch, &QAbstractButton::toggled, this,
          &QEventDetailsWidget::onOnlineSessionToggled);
  connect(mMeetingUrlEdit, &QLineEdit::textChanged, this,
          &QEventDetailsWidget::onMeetingUrlChanged);
  connect(mOpenMeetingButton, &QPushButton::clicked, this,
          &QEventDetailsWidget::onOpenMeetingClicked);
  connect(mCopyMeetingUrlButton, &QPushButton::clicked, this,
          &QEventDetailsWidget::onCopyMeetingUrlClicked);
  connect(mCopyMeetingInviteButton, &QPushButton::clicked, this,
          &QEventDetailsWidget::onCopyMeetingInviteClicked);
  connect(mUI->mTimeFrom, &QTimeEdit::timeChanged, this,
          &QEventDetailsWidget::onTimeFromChanged);
  connect(mUI->mTimeTo, &QTimeEdit::timeChanged, this,
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
  mUI->mCostLabel->setVisible(isVisible);
  mUI->mCostSpinBox->setVisible(isVisible);
  mUI->mPaymentStatusLabel->setVisible(isVisible);
  mUI->mPaymentStatusComboBox->setVisible(isVisible);
  mEventTypeSwitch->setEnabled(false);
  mUI->mButtonBox->setVisible(false);
  mUI->mChangeButton->setVisible(true);
  mUI->mAddButton->setVisible(true);
  onOnlineSessionToggled(mOnlineSessionSwitch->isChecked());
  emit provideFillClientComboBox(mUI->mClientComboBox);
}

void QEventDetailsWidget::initEditStyle() {
  mUI->mButtonBox->setVisible(true);
  mUI->mChangeButton->setVisible(false);
  mUI->mAddButton->setVisible(false);
  mEventTypeSwitch->setEnabled(true);
  onEventTypeToggled(mEventTypeSwitch->isChecked());
  onOnlineSessionToggled(mOnlineSessionSwitch->isChecked());
  emit provideFillClientComboBox(mUI->mClientComboBox);
}

void QEventDetailsWidget::initDefaultStates() const {
  // Set initial text for checkbox
  mEventTypeSwitch->setText(tr(": EVENT_TYPE_REGULAR"));
}

void QEventDetailsWidget::initDefaultTimes() const {
  const auto crtDateTime = QDateTime::currentDateTime();
  mUI->mEventDate->setDate(crtDateTime.date());
  mUI->mTimeFrom->setTime(crtDateTime.time());
  mUI->mTimeTo->setTime(crtDateTime.addSecs(3600).time()); // +1 hour
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
  mUI->mTimeFrom->setTime(event->getStartTime().time());
  mUI->mTimeTo->setTime(event->getEndTime().time());
  const bool isWorkItem = event->isWorkItem();
  mEventTypeSwitch->setChecked(isWorkItem);
  mOnlineSessionSwitch->setChecked(event->isOnline());
  mMeetingUrlEdit->setText(event->meetingUrl());
  mUI->mCostSpinBox->setValue(
      event->cost().value_or(pcm::app_settings::defaultWorkEventCost()));
  const auto paymentStatusData = QVariant::fromValue(event->paymentStatusId());
  if (const int paymentIndex =
          mUI->mPaymentStatusComboBox->findData(paymentStatusData);
      paymentIndex != -1) {
    mUI->mPaymentStatusComboBox->setCurrentIndex(paymentIndex);
  }

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
  onEventTypeToggled(isWorkItem);
  onOnlineSessionToggled(event->isOnline());
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

void QEventDetailsWidget::startCreatingNewEvent(const QDate &date,
                                                const std::optional<QTime> startTime,
                                                const std::optional<int> durationMinutes) {
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
  if (startTime.has_value()) {
    const auto duration = durationMinutes.value_or(
        pcm::app_settings::defaultSessionDurationMinutes());
    mUI->mTimeFrom->setTime(*startTime);
    mUI->mTimeTo->setTime(startTime->addSecs(duration * 60));
  }
  mUI->mCostSpinBox->setValue(pcm::app_settings::defaultWorkEventCost());
  mUI->mPaymentStatusComboBox->setCurrentIndex(
      mUI->mPaymentStatusComboBox->findData(QVariant::fromValue(kPaymentPendingId)));
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

int64_t QEventDetailsWidget::selectedClientId() const {
  if (!mEventTypeSwitch->isChecked()) {
    return 0;
  }

  return mUI->mClientComboBox->currentData().toLongLong();
}

QString QEventDetailsWidget::selectedClientName() const {
  if (!mEventTypeSwitch->isChecked()) {
    return {};
  }

  return mUI->mClientComboBox->currentText();
}

void QEventDetailsWidget::setConflictChecker(
    std::function<bool(const DuckEvent &)> checker) {
  mConflictChecker = std::move(checker);
}

QEventItem *QEventDetailsWidget::currentEvent() const {
  return mCurrentEvent.data();
}

void QEventDetailsWidget::onApplyClicked() {
  if (!validateInput()) {
    return;
  }

  if (mCurrentEvent) {
    const auto startDateTime = QDateTime(mUI->mEventDate->date(),
                                         mUI->mTimeFrom->time(),
                                         QTimeZone::systemTimeZone());
    const auto endDateTime = QDateTime(mUI->mEventDate->date(),
                                       mUI->mTimeTo->time(),
                                       QTimeZone::systemTimeZone());
    qCDebug(logEventDetails) << "Apply event from form:"
                             << "date=" << mUI->mEventDate->date()
                             << "timeFrom=" << mUI->mTimeFrom->time()
                             << "timeTo=" << mUI->mTimeTo->time()
                             << "startLocal=" << startDateTime.toString(Qt::ISODate)
                             << "startUtc=" << startDateTime.toUTC().toString(Qt::ISODate)
                             << "startMs=" << startDateTime.toMSecsSinceEpoch()
                             << "endLocal=" << endDateTime.toString(Qt::ISODate)
                             << "endUtc=" << endDateTime.toUTC().toString(Qt::ISODate)
                             << "endMs=" << endDateTime.toMSecsSinceEpoch();

    mCurrentEvent->setTitle(mUI->mTitle->text());
    mCurrentEvent->setTimeRange(startDateTime, endDateTime);
    mCurrentEvent->setIsWorkItem(mEventTypeSwitch->isChecked());
    mCurrentEvent->setCost(mEventTypeSwitch->isChecked()
                               ? std::make_optional(mUI->mCostSpinBox->value())
                               : std::nullopt);
    mCurrentEvent->setPaymentStatusId(
        mEventTypeSwitch->isChecked()
            ? mUI->mPaymentStatusComboBox->currentData().toLongLong()
            : kPaymentSkippedId);
    mCurrentEvent->setOnline(mOnlineSessionSwitch->isChecked());
    mCurrentEvent->setMeetingUrl(mOnlineSessionSwitch->isChecked()
                                     ? mMeetingUrlEdit->text()
                                     : QString{});
  }

  if (mCurrentEvent) {
    const auto eventData = mCurrentEvent->toEvent();
    if (mConflictChecker && mConflictChecker(eventData)) {
      QMessageBox::warning(
          this, tr(": ERROR_TITLE"),
          tr("The selected time range overlaps an existing event."));
      return;
    }
  }

  const bool isCreatingNewEvent = mCreatingNewEvent;

  // Emit signal to save the event
  emit provideEventSave(mCurrentEvent.data());

  if (isCreatingNewEvent && (!mCurrentEvent || mCurrentEvent->getId() <= 0)) {
    QMessageBox::warning(this, tr(": ERROR_TITLE"),
                         tr("Failed to save event to database"));
    return;
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
  emit provideDialogAccept();
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
  mEventTypeSwitch->setText(
      checked ? tr(": EVENT_TYPE_WORK") : tr(": EVENT_TYPE_REGULAR"));
  mUI->mClientComboBox->setVisible(checked);
  mUI->mClientComboxBoxLabel->setVisible(checked);
  mUI->mCostLabel->setVisible(checked);
  mUI->mCostSpinBox->setVisible(checked);
  mUI->mPaymentStatusLabel->setVisible(checked);
  mUI->mPaymentStatusComboBox->setVisible(checked);
  if (checked && mCreatingNewEvent && mUI->mCostSpinBox->value() <= 0.0) {
    mUI->mCostSpinBox->setValue(pcm::app_settings::defaultWorkEventCost());
  }
  if (!checked) {
    const auto skippedIndex =
        mUI->mPaymentStatusComboBox->findData(QVariant::fromValue(kPaymentSkippedId));
    if (skippedIndex != -1) {
      mUI->mPaymentStatusComboBox->setCurrentIndex(skippedIndex);
    }
  } else if (mUI->mPaymentStatusComboBox->currentData().toLongLong() == kPaymentSkippedId) {
    const auto pendingIndex =
        mUI->mPaymentStatusComboBox->findData(QVariant::fromValue(kPaymentPendingId));
    if (pendingIndex != -1) {
      mUI->mPaymentStatusComboBox->setCurrentIndex(pendingIndex);
    }
  }
}

void QEventDetailsWidget::onOnlineSessionToggled(const bool checked) {
  mMeetingUrlLabel->setVisible(checked);
  mMeetingUrlEdit->setVisible(checked);
  mMeetingActionsWidget->setVisible(checked);
  updateButtonState();
}

void QEventDetailsWidget::onMeetingUrlChanged(const QString &url) {
  Q_UNUSED(url)
  updateButtonState();
}

void QEventDetailsWidget::onOpenMeetingClicked() {
  pcm::meeting::openMeetingUrl(mMeetingUrlEdit->text(), this);
}

void QEventDetailsWidget::onCopyMeetingUrlClicked() {
  if (!pcm::meeting::isValidMeetingUrl(mMeetingUrlEdit->text())) {
    QMessageBox::warning(this, tr(": ERROR_TITLE"),
                         tr("Enter a valid http or https meeting link."));
    return;
  }

  pcm::meeting::copyMeetingUrl(mMeetingUrlEdit->text());
}

void QEventDetailsWidget::onCopyMeetingInviteClicked() {
  if (!pcm::meeting::isValidMeetingUrl(mMeetingUrlEdit->text())) {
    QMessageBox::warning(this, tr(": ERROR_TITLE"),
                         tr("Enter a valid http or https meeting link."));
    return;
  }

  const auto event = collectEventData();
  pcm::meeting::copyMeetingInvite(QString::fromStdString(event.meeting_url),
                                  selectedClientName(),
                                  event.start_date.value_or(0));
}

void QEventDetailsWidget::onTimeFromChanged(const QTime &timeFrom) {
  if (mUI->mTimeTo->time() < timeFrom) {
    mUI->mTimeTo->setTime(timeFrom.addSecs(60)); // At least 1 minute
  }
  updateButtonState();
}

void QEventDetailsWidget::onTimeToChanged(const QTime &timeTo) {
  if (timeTo < mUI->mTimeFrom->time()) {
    mUI->mTimeFrom->setTime(timeTo.addSecs(-60));
  }
  updateButtonState();
}

void QEventDetailsWidget::updateButtonState() const {
  const bool isValid = !mUI->mTitle->text().trimmed().isEmpty() &&
                       mUI->mTimeTo->time() > mUI->mTimeFrom->time();
  if (auto *applyButton = mUI->mButtonBox->button(QDialogButtonBox::Apply)) {
    applyButton->setEnabled(isValid);
  }
  const bool hasValidMeetingUrl = pcm::meeting::isValidMeetingUrl(mMeetingUrlEdit->text());
  mOpenMeetingButton->setEnabled(hasValidMeetingUrl);
  mCopyMeetingUrlButton->setEnabled(hasValidMeetingUrl);
  mCopyMeetingInviteButton->setEnabled(hasValidMeetingUrl);
}

bool QEventDetailsWidget::validateInput() {
  const bool isTitleEmpty = mUI->mTitle->text().trimmed().isEmpty();
  const bool isZeroDuration = mUI->mTimeTo->time() <= mUI->mTimeFrom->time();

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
  if (mOnlineSessionSwitch->isChecked()) {
    const auto meetingUrl = mMeetingUrlEdit->text().trimmed();
    if (meetingUrl.isEmpty()) {
      const auto answer = QMessageBox::question(
          this, tr("Online session"),
          tr("Online session is enabled, but the meeting link is empty. Save without a link?"),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (answer != QMessageBox::Yes) {
        return false;
      }
    } else if (!pcm::meeting::isValidMeetingUrl(meetingUrl)) {
      QMessageBox::warning(this, tr(": ERROR_TITLE"),
                           tr("Enter a valid http or https meeting link."));
      return false;
    }
  }
  return true;
}

DuckEvent QEventDetailsWidget::collectEventData() const {
  // This method is no longer used in the main logic flow.
  // It is left for backward compatibility or internal use if needed.
  DuckEvent event;
  event.name = mUI->mTitle->text().toStdString();
  event.start_date = QDateTime(mUI->mEventDate->date(), mUI->mTimeFrom->time(),
                               QTimeZone::systemTimeZone())
                         .toMSecsSinceEpoch();
  event.end_date = QDateTime(mUI->mEventDate->date(), mUI->mTimeTo->time(),
                             QTimeZone::systemTimeZone())
                       .toMSecsSinceEpoch();
  event.is_work_event = mEventTypeSwitch->isChecked();
  event.duration = (event.end_date.value_or(0) - event.start_date.value_or(0)) / 1000; // in seconds
  event.cost = event.is_work_event ? std::make_optional(mUI->mCostSpinBox->value())
                                   : std::nullopt;
  event.payment_stat_id =
      event.is_work_event ? mUI->mPaymentStatusComboBox->currentData().toLongLong()
                          : kPaymentSkippedId;
  event.is_online = mOnlineSessionSwitch->isChecked();
  event.meeting_url =
      event.is_online ? mMeetingUrlEdit->text().trimmed().toStdString() : std::string{};
  return event;
}
