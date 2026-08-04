#include "settings_dialog.h"

#include "../widgets/app_settings.h"
#include "backup_service.h"
#include "backup_validator.h"
#include "restore_service.h"

#include <oclero/qlementine/widgets/ColorEditor.hpp>
#include <oclero/qlementine/widgets/SegmentedControl.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextEdit>
#include <QThread>
#include <QTimeEdit>
#include <QUrl>
#include <QVBoxLayout>

#include <Poco/Path.h>

namespace {
QWidget *makeSettingRow(const QString &title, const QString &description,
                        QWidget *control,
                        QWidget *parent = nullptr) {
  auto *row = new QWidget(parent);
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);

  auto *textLayout = new QVBoxLayout();
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->setSpacing(2);

  auto *titleLabel = new QLabel(title, row);
  QFont titleFont = titleLabel->font();
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);

  auto *descriptionLabel = new QLabel(description, row);
  descriptionLabel->setWordWrap(true);
  descriptionLabel->setStyleSheet("color: rgba(255, 255, 255, 0.68);");

  textLayout->addWidget(titleLabel);
  textLayout->addWidget(descriptionLabel);

  layout->addLayout(textLayout, 1);
  layout->addWidget(control, 0, Qt::AlignTop);

  return row;
}

class BackupWorker final : public QObject {
  Q_OBJECT

public:
  BackupWorker(std::shared_ptr<pcm::database::Database> db,
              QString destinationPath, QString attachmentsRoot)
      : mDb(std::move(db)), mDestinationPath(std::move(destinationPath)),
        mAttachmentsRoot(std::move(attachmentsRoot)) {}

public slots:
  void run() {
    pcm::backup::BackupService service;
    pcm::backup::BackupOptions options;
    options.attachments_root = mAttachmentsRoot.toStdString();
    const auto result =
        service.create_backup(*mDb, mDestinationPath.toStdString(), options);
    emit finished(result.ok, QString::fromStdString(result.error));
  }

signals:
  void finished(bool ok, const QString &error);

private:
  std::shared_ptr<pcm::database::Database> mDb;
  QString mDestinationPath;
  QString mAttachmentsRoot;
};

class ValidateWorker final : public QObject {
  Q_OBJECT

public:
  explicit ValidateWorker(QString backupPath)
      : mBackupPath(std::move(backupPath)) {}

public slots:
  void run() {
    pcm::backup::BackupValidator validator;
    const auto result = validator.validate(mBackupPath.toStdString());
    QStringList errors;
    errors.reserve(static_cast<int>(result.errors.size()));
    for (const auto &error : result.errors) {
      errors << QString::fromStdString(error);
    }
    emit finished(result.ok, errors);
  }

signals:
  void finished(bool ok, const QStringList &errors);

private:
  QString mBackupPath;
};
} // namespace

SettingsDialog::SettingsDialog(std::shared_ptr<pcm::database::Database> db,
                               QWidget *parent)
    : QDialog(parent), mDb(std::move(db)) {
  setupUi();
  loadSettings();
  connectSignals();
}

void SettingsDialog::setupUi() {
  setWindowTitle(tr("Settings"));
  setModal(true);
  resize(560, 760);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(20, 20, 20, 20);
  rootLayout->setSpacing(16);

  auto *caption = new QLabel(tr("Application settings"), this);
  QFont captionFont = caption->font();
  captionFont.setPointSize(captionFont.pointSize() + 1);
  captionFont.setBold(true);
  caption->setFont(captionFont);
  rootLayout->addWidget(caption);

  mSettingsSections = new oclero::qlementine::SegmentedControl(this);
  mSettingsSections->addItem(tr("General"), {}, {}, QStringLiteral("general"));
  mSettingsSections->addItem(tr("Events"), {}, {}, QStringLiteral("events"));
  mSettingsSections->addItem(tr("Online"), {}, {}, QStringLiteral("online"));
  mSettingsSections->setItemsShouldExpand(true);
  rootLayout->addWidget(mSettingsSections);

  mSettingsStack = new QStackedWidget(this);
  rootLayout->addWidget(mSettingsStack, 1);

  auto *generalPage = new QWidget(mSettingsStack);
  auto *generalSettingsLayout = new QVBoxLayout(generalPage);
  generalSettingsLayout->setContentsMargins(0, 0, 0, 0);
  generalSettingsLayout->setSpacing(16);

  auto *eventsPage = new QWidget(mSettingsStack);
  auto *eventSettingsLayout = new QVBoxLayout(eventsPage);
  eventSettingsLayout->setContentsMargins(0, 0, 0, 0);
  eventSettingsLayout->setSpacing(16);

  auto *onlinePage = new QWidget(mSettingsStack);
  auto *onlineSettingsLayout = new QVBoxLayout(onlinePage);
  onlineSettingsLayout->setContentsMargins(0, 0, 0, 0);
  onlineSettingsLayout->setSpacing(16);

  mSettingsStack->addWidget(generalPage);
  mSettingsStack->addWidget(eventsPage);
  mSettingsStack->addWidget(onlinePage);

  auto *languageBox = new QGroupBox(tr("Language"), generalPage);
  auto *languageLayout = new QVBoxLayout(languageBox);
  languageLayout->setContentsMargins(16, 16, 16, 16);
  languageLayout->setSpacing(14);
  mLanguageCombo = new QComboBox(languageBox);
  mLanguageCombo->addItem(tr("System default"), QStringLiteral("system"));
  mLanguageCombo->addItem(tr("Russian"), QStringLiteral("ru"));
  mLanguageCombo->addItem(tr("English"), QStringLiteral("en"));
  languageLayout->addWidget(
      makeSettingRow(tr("Interface language"),
                     tr("The selected language will be applied after restarting the application."),
                     mLanguageCombo, languageBox));
  generalSettingsLayout->addWidget(languageBox);

  auto *databaseBox = new QGroupBox(tr("Database"), generalPage);
  auto *databaseLayout = new QVBoxLayout(databaseBox);
  databaseLayout->setContentsMargins(16, 16, 16, 16);
  databaseLayout->setSpacing(10);
  auto *dbPathTitle = new QLabel(tr("Database directory"), databaseBox);
  QFont dbPathTitleFont = dbPathTitle->font();
  dbPathTitleFont.setBold(true);
  dbPathTitle->setFont(dbPathTitleFont);
  mDatabasePathLabel = new QLabel(databaseBox);
  mDatabasePathLabel->setWordWrap(true);
  mDatabasePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  mDatabasePathLabel->setStyleSheet("color: rgba(255, 255, 255, 0.68);");
  mOpenDatabaseFolderButton = new QPushButton(tr("Open folder"), databaseBox);
  databaseLayout->addWidget(dbPathTitle);
  databaseLayout->addWidget(mDatabasePathLabel);
  databaseLayout->addWidget(mOpenDatabaseFolderButton, 0, Qt::AlignLeft);
  generalSettingsLayout->addWidget(databaseBox);

  auto *backupBox = new QGroupBox(tr("Backup"), generalPage);
  auto *backupLayout = new QVBoxLayout(backupBox);
  backupLayout->setContentsMargins(16, 16, 16, 16);
  backupLayout->setSpacing(10);
  auto *backupDescription = new QLabel(
      tr("Create a full backup (database and attachments) as a single "
        ".psybackup file, or validate an existing one."),
      backupBox);
  backupDescription->setWordWrap(true);
  backupDescription->setStyleSheet("color: rgba(255, 255, 255, 0.68);");
  auto *backupButtonsRow = new QWidget(backupBox);
  auto *backupButtonsLayout = new QHBoxLayout(backupButtonsRow);
  backupButtonsLayout->setContentsMargins(0, 0, 0, 0);
  backupButtonsLayout->setSpacing(10);
  mCreateBackupButton = new QPushButton(tr("Create backup..."), backupBox);
  mValidateBackupButton = new QPushButton(tr("Validate backup..."), backupBox);
  mRestoreBackupButton = new QPushButton(tr("Restore backup..."), backupBox);
  backupButtonsLayout->addWidget(mCreateBackupButton);
  backupButtonsLayout->addWidget(mValidateBackupButton);
  backupButtonsLayout->addWidget(mRestoreBackupButton);
  backupButtonsLayout->addStretch();
  mBackupStatusLabel = new QLabel(backupBox);
  mBackupStatusLabel->setStyleSheet("color: rgba(255, 255, 255, 0.68);");
  mBackupStatusLabel->setVisible(false);
  mBackupProgressBar = new QProgressBar(backupBox);
  mBackupProgressBar->setRange(0, 0);
  mBackupProgressBar->setTextVisible(false);
  mBackupProgressBar->setVisible(false);
  backupLayout->addWidget(backupDescription);
  backupLayout->addWidget(backupButtonsRow);
  backupLayout->addWidget(mBackupStatusLabel);
  backupLayout->addWidget(mBackupProgressBar);
  generalSettingsLayout->addWidget(backupBox);

  auto *autoBackupBox = new QGroupBox(tr("Automatic Backups"), generalPage);
  auto *autoBackupLayout = new QVBoxLayout(autoBackupBox);
  autoBackupLayout->setContentsMargins(16, 16, 16, 16);
  autoBackupLayout->setSpacing(14);
  mAutoBackupEnabledSwitch = new oclero::qlementine::Switch(autoBackupBox);
  mAutoBackupIntervalSpinBox = new QSpinBox(autoBackupBox);
  mAutoBackupIntervalSpinBox->setMinimum(1);
  mAutoBackupIntervalSpinBox->setMaximum(90);
  mAutoBackupIntervalSpinBox->setSuffix(tr(" days"));
  mAutoBackupKeepCountSpinBox = new QSpinBox(autoBackupBox);
  mAutoBackupKeepCountSpinBox->setMinimum(1);
  mAutoBackupKeepCountSpinBox->setMaximum(50);
  auto *destinationRow = new QWidget(autoBackupBox);
  auto *destinationLayout = new QHBoxLayout(destinationRow);
  destinationLayout->setContentsMargins(0, 0, 0, 0);
  destinationLayout->setSpacing(10);
  mAutoBackupDestinationEdit = new QLineEdit(destinationRow);
  mAutoBackupDestinationEdit->setReadOnly(true);
  mAutoBackupBrowseButton = new QPushButton(tr("Browse..."), destinationRow);
  destinationLayout->addWidget(mAutoBackupDestinationEdit, 1);
  destinationLayout->addWidget(mAutoBackupBrowseButton);
  autoBackupLayout->addWidget(makeSettingRow(
      tr("Automatic backups"),
      tr("Periodically create a backup in the background without needing "
        "to click \"Create backup...\"."),
      mAutoBackupEnabledSwitch, autoBackupBox));
  autoBackupLayout->addWidget(
      makeSettingRow(tr("Backup interval"), tr("How often an automatic backup is taken."),
                    mAutoBackupIntervalSpinBox, autoBackupBox));
  autoBackupLayout->addWidget(makeSettingRow(
      tr("Keep last"),
      tr("How many automatic backups to keep before older ones are deleted."),
      mAutoBackupKeepCountSpinBox, autoBackupBox));
  autoBackupLayout->addWidget(makeSettingRow(tr("Destination folder"),
                                             tr("Where automatic backups are saved."),
                                             destinationRow, autoBackupBox));
  generalSettingsLayout->addWidget(autoBackupBox);

  auto *notificationsBox = new QGroupBox(tr("Notifications"), generalPage);
  auto *notificationsLayout = new QVBoxLayout(notificationsBox);
  notificationsLayout->setContentsMargins(16, 16, 16, 16);
  notificationsLayout->setSpacing(14);
  mNotificationsEnabledSwitch = new oclero::qlementine::Switch(notificationsBox);
  mNotificationLeadMinutesSpinBox = new QSpinBox(notificationsBox);
  mNotificationLeadMinutesSpinBox->setMinimum(1);
  mNotificationLeadMinutesSpinBox->setMaximum(24 * 60);
  mNotificationLeadMinutesSpinBox->setSingleStep(5);
  mNotificationLeadMinutesSpinBox->setSuffix(tr(" min"));
  notificationsLayout->addWidget(
      makeSettingRow(tr("Session reminders"),
                     tr("Show a desktop notification before a scheduled session starts."),
                     mNotificationsEnabledSwitch, notificationsBox));
  notificationsLayout->addWidget(
      makeSettingRow(tr("Notify before start"),
                     tr("How many minutes before the session the reminder should appear."),
                     mNotificationLeadMinutesSpinBox, notificationsBox));
  generalSettingsLayout->addWidget(notificationsBox);
  generalSettingsLayout->addStretch();

  auto *eventsBox = new QGroupBox(tr("Timeline colors"), eventsPage);
  auto *eventsLayout = new QVBoxLayout(eventsBox);
  eventsLayout->setContentsMargins(16, 16, 16, 16);
  eventsLayout->setSpacing(14);
  mPreventOverlapsSwitch = new oclero::qlementine::Switch(eventsBox);
  mWorkEventColorEditor = new oclero::qlementine::ColorEditor(eventsBox);
  mPersonalEventColorEditor = new oclero::qlementine::ColorEditor(eventsBox);
  mCurrencyCombo = new QComboBox(eventsBox);
  mCurrencyCombo->addItem(tr("Russian Ruble (₽)"), QStringLiteral("RUB"));
  mCurrencyCombo->addItem(tr("US Dollar ($)"), QStringLiteral("USD"));
  mCurrencyCombo->addItem(tr("Euro (€)"), QStringLiteral("EUR"));
  mCurrencyCombo->addItem(tr("British Pound (£)"), QStringLiteral("GBP"));
  mDefaultWorkCostSpinBox = new QDoubleSpinBox(eventsBox);
  mDefaultWorkCostSpinBox->setDecimals(2);
  mDefaultWorkCostSpinBox->setMinimum(0.0);
  mDefaultWorkCostSpinBox->setMaximum(1'000'000.0);
  mDefaultWorkCostSpinBox->setSingleStep(100.0);
  mDefaultWorkCostSpinBox->setSuffix(QStringLiteral(" ") + pcm::app_settings::currencySymbol());
  mWorkDayStartEdit = new QTimeEdit(eventsBox);
  mWorkDayStartEdit->setDisplayFormat("HH:mm");
  mWorkDayEndEdit = new QTimeEdit(eventsBox);
  mWorkDayEndEdit->setDisplayFormat("HH:mm");
  mDefaultSessionDurationSpinBox = new QSpinBox(eventsBox);
  mDefaultSessionDurationSpinBox->setMinimum(5);
  mDefaultSessionDurationSpinBox->setMaximum(480);
  mDefaultSessionDurationSpinBox->setSingleStep(5);
  mDefaultSessionDurationSpinBox->setSuffix(tr(" min"));
  mDefaultBufferBeforeSpinBox = new QSpinBox(eventsBox);
  mDefaultBufferBeforeSpinBox->setRange(0, 240);
  mDefaultBufferBeforeSpinBox->setSuffix(tr(" min"));
  mDefaultBufferAfterSpinBox = new QSpinBox(eventsBox);
  mDefaultBufferAfterSpinBox->setRange(0, 240);
  mDefaultBufferAfterSpinBox->setSuffix(tr(" min"));
  eventsLayout->addWidget(
      makeSettingRow(tr("Disallow overlapping events"),
                     tr("Reject saves when the selected time range intersects another event."),
                     mPreventOverlapsSwitch, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Work day start"),
                     tr("Start time used for quick session suggestions."),
                     mWorkDayStartEdit, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Work day end"),
                     tr("End time used for quick session suggestions."),
                     mWorkDayEndEdit, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Default session duration"),
                     tr("Duration used for quick session suggestions and new sessions."),
                     mDefaultSessionDurationSpinBox, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Default buffer before"),
                     tr("Time reserved before each new session and Quick Slot."),
                     mDefaultBufferBeforeSpinBox, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Default buffer after"),
                     tr("Time reserved after each new session and Quick Slot."),
                     mDefaultBufferAfterSpinBox, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Currency"),
                     tr("Symbol shown next to cost values throughout the app."),
                     mCurrencyCombo, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Default work event cost"),
                     tr("Used to prefill new work sessions."),
                     mDefaultWorkCostSpinBox, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Work events"),
                     tr("Accent color for work sessions in the timeline."),
                     mWorkEventColorEditor, eventsBox));
  eventsLayout->addWidget(
      makeSettingRow(tr("Personal events"),
                     tr("Accent color for personal events in the timeline."),
                     mPersonalEventColorEditor, eventsBox));
  eventSettingsLayout->addWidget(eventsBox);

  auto *confirmationBox = new QGroupBox(tr("Confirmation request template"), eventsPage);
  auto *confirmationLayout = new QVBoxLayout(confirmationBox);
  confirmationLayout->setContentsMargins(16, 16, 16, 16);
  confirmationLayout->setSpacing(10);
  auto *confirmationTemplateDescription = new QLabel(
      tr("Available variables: {client_name}, {date}, {time}"), confirmationBox);
  confirmationTemplateDescription->setWordWrap(true);
  confirmationTemplateDescription->setStyleSheet("color: rgba(255, 255, 255, 0.68);");
  mConfirmationRequestTemplateEdit = new QTextEdit(confirmationBox);
  mConfirmationRequestTemplateEdit->setAcceptRichText(false);
  mConfirmationRequestTemplateEdit->setMinimumHeight(100);
  confirmationLayout->addWidget(confirmationTemplateDescription);
  confirmationLayout->addWidget(mConfirmationRequestTemplateEdit);
  eventSettingsLayout->addWidget(confirmationBox);
  eventSettingsLayout->addStretch();

  auto *onlineBox = new QGroupBox(tr("Online sessions"), onlinePage);
  auto *onlineLayout = new QVBoxLayout(onlineBox);
  onlineLayout->setContentsMargins(16, 16, 16, 16);
  onlineLayout->setSpacing(10);
  auto *templateTitle = new QLabel(tr("Invite template"), onlineBox);
  QFont templateTitleFont = templateTitle->font();
  templateTitleFont.setBold(true);
  templateTitle->setFont(templateTitleFont);
  auto *templateDescription = new QLabel(
      tr("Available variables: {client_name}, {date}, {time}, {meeting_url}"),
      onlineBox);
  templateDescription->setWordWrap(true);
  templateDescription->setStyleSheet("color: rgba(255, 255, 255, 0.68);");
  mMeetingInviteTemplateEdit = new QTextEdit(onlineBox);
  mMeetingInviteTemplateEdit->setAcceptRichText(false);
  mMeetingInviteTemplateEdit->setMinimumHeight(120);
  onlineLayout->addWidget(templateTitle);
  onlineLayout->addWidget(templateDescription);
  onlineLayout->addWidget(mMeetingInviteTemplateEdit);
  onlineSettingsLayout->addWidget(onlineBox);
  onlineSettingsLayout->addStretch();

  mButtonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
  rootLayout->addWidget(mButtonBox);
}

void SettingsDialog::loadSettings() const {
  const auto languageCode = pcm::app_settings::languageCode();
  const auto languageIndex = mLanguageCombo->findData(languageCode);
  mLanguageCombo->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
  mDatabasePathLabel->setText(
      QString::fromStdString(mConfig.db_conf.value_.db_pth.toString()));
  mNotificationsEnabledSwitch->setChecked(pcm::app_settings::notificationsEnabled());
  mNotificationLeadMinutesSpinBox->setValue(
      pcm::app_settings::notificationLeadMinutes());
  mNotificationLeadMinutesSpinBox->setEnabled(
      mNotificationsEnabledSwitch->isChecked());
  mAutoBackupEnabledSwitch->setChecked(pcm::app_settings::autoBackupEnabled());
  mAutoBackupIntervalSpinBox->setValue(pcm::app_settings::autoBackupIntervalDays());
  mAutoBackupKeepCountSpinBox->setValue(pcm::app_settings::autoBackupKeepCount());
  mAutoBackupDestinationEdit->setText(pcm::app_settings::autoBackupDestination());
  const auto autoBackupEnabled = mAutoBackupEnabledSwitch->isChecked();
  mAutoBackupIntervalSpinBox->setEnabled(autoBackupEnabled);
  mAutoBackupKeepCountSpinBox->setEnabled(autoBackupEnabled);
  mAutoBackupDestinationEdit->setEnabled(autoBackupEnabled);
  mAutoBackupBrowseButton->setEnabled(autoBackupEnabled);
  mPreventOverlapsSwitch->setChecked(pcm::app_settings::preventEventOverlaps());
  mWorkDayStartEdit->setTime(pcm::app_settings::workDayStart());
  mWorkDayEndEdit->setTime(pcm::app_settings::workDayEnd());
  mDefaultSessionDurationSpinBox->setValue(
      pcm::app_settings::defaultSessionDurationMinutes());
  mDefaultBufferBeforeSpinBox->setValue(
      pcm::app_settings::defaultBufferBeforeMinutes());
  mDefaultBufferAfterSpinBox->setValue(
      pcm::app_settings::defaultBufferAfterMinutes());
  const auto currencyIndex = mCurrencyCombo->findData(pcm::app_settings::currencyCode());
  mCurrencyCombo->setCurrentIndex(currencyIndex >= 0 ? currencyIndex : 0);
  mDefaultWorkCostSpinBox->setValue(pcm::app_settings::defaultWorkEventCost());
  mWorkEventColorEditor->setColor(pcm::app_settings::workEventColor());
  mPersonalEventColorEditor->setColor(pcm::app_settings::personalEventColor());
  mMeetingInviteTemplateEdit->setPlainText(
      pcm::app_settings::meetingInviteTemplate());
  mConfirmationRequestTemplateEdit->setPlainText(
      pcm::app_settings::confirmationRequestTemplate());
}

void SettingsDialog::connectSignals() const {
  connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
  connect(mSettingsSections, &oclero::qlementine::SegmentedControl::currentIndexChanged,
          this, [this]() {
            mSettingsStack->setCurrentIndex(mSettingsSections->currentIndex());
          });
  connect(mLanguageCombo, &QComboBox::currentIndexChanged, this, [this](const int index) {
    pcm::app_settings::setLanguageCode(mLanguageCombo->itemData(index).toString());
  });
  connect(mOpenDatabaseFolderButton, &QPushButton::clicked, this,
          &SettingsDialog::openDatabaseFolder);
  connect(mCreateBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::createBackup);
  connect(mValidateBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::validateBackup);
  connect(mRestoreBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::restoreBackup);
  connect(mNotificationsEnabledSwitch, &QAbstractButton::toggled, this,
          [this](const bool checked) {
            pcm::app_settings::setNotificationsEnabled(checked);
            mNotificationLeadMinutesSpinBox->setEnabled(checked);
          });
  connect(mNotificationLeadMinutesSpinBox, &QSpinBox::valueChanged, this,
          [](const int minutes) {
            pcm::app_settings::setNotificationLeadMinutes(minutes);
          });
  connect(mAutoBackupEnabledSwitch, &QAbstractButton::toggled, this,
          [this](const bool checked) {
            pcm::app_settings::setAutoBackupEnabled(checked);
            mAutoBackupIntervalSpinBox->setEnabled(checked);
            mAutoBackupKeepCountSpinBox->setEnabled(checked);
            mAutoBackupDestinationEdit->setEnabled(checked);
            mAutoBackupBrowseButton->setEnabled(checked);
          });
  connect(mAutoBackupIntervalSpinBox, &QSpinBox::valueChanged, this,
          [](const int days) {
            pcm::app_settings::setAutoBackupIntervalDays(days);
          });
  connect(mAutoBackupKeepCountSpinBox, &QSpinBox::valueChanged, this,
          [](const int count) {
            pcm::app_settings::setAutoBackupKeepCount(count);
          });
  connect(mAutoBackupBrowseButton, &QPushButton::clicked, this,
          &SettingsDialog::browseAutoBackupDestination);
  connect(mPreventOverlapsSwitch, &QAbstractButton::toggled, this,
          [](const bool checked) {
            pcm::app_settings::setPreventEventOverlaps(checked);
          });
  connect(mWorkDayStartEdit, &QTimeEdit::timeChanged, this,
          [](const QTime &time) {
            pcm::app_settings::setWorkDayStart(time);
          });
  connect(mWorkDayEndEdit, &QTimeEdit::timeChanged, this,
          [](const QTime &time) {
            pcm::app_settings::setWorkDayEnd(time);
          });
  connect(mDefaultSessionDurationSpinBox, &QSpinBox::valueChanged, this,
          [](const int minutes) {
            pcm::app_settings::setDefaultSessionDurationMinutes(minutes);
          });
  connect(mDefaultBufferBeforeSpinBox, &QSpinBox::valueChanged, this,
          [](const int minutes) {
            pcm::app_settings::setDefaultBufferBeforeMinutes(minutes);
          });
  connect(mDefaultBufferAfterSpinBox, &QSpinBox::valueChanged, this,
          [](const int minutes) {
            pcm::app_settings::setDefaultBufferAfterMinutes(minutes);
          });
  connect(mCurrencyCombo, &QComboBox::currentIndexChanged, this, [this](const int index) {
    pcm::app_settings::setCurrencyCode(mCurrencyCombo->itemData(index).toString());
    mDefaultWorkCostSpinBox->setSuffix(QStringLiteral(" ") + pcm::app_settings::currencySymbol());
  });
  connect(mDefaultWorkCostSpinBox, &QDoubleSpinBox::valueChanged, this,
          [](const double value) {
            pcm::app_settings::setDefaultWorkEventCost(value);
          });
  connect(mWorkEventColorEditor, &oclero::qlementine::ColorEditor::colorChanged,
          this, [this]() {
            pcm::app_settings::setWorkEventColor(mWorkEventColorEditor->color());
          });
  connect(mPersonalEventColorEditor, &oclero::qlementine::ColorEditor::colorChanged,
          this, [this]() {
            pcm::app_settings::setPersonalEventColor(mPersonalEventColorEditor->color());
          });
  connect(mMeetingInviteTemplateEdit, &QTextEdit::textChanged, this, [this]() {
    pcm::app_settings::setMeetingInviteTemplate(
        mMeetingInviteTemplateEdit->toPlainText());
  });
  connect(mConfirmationRequestTemplateEdit, &QTextEdit::textChanged, this, [this]() {
    pcm::app_settings::setConfirmationRequestTemplate(
        mConfirmationRequestTemplateEdit->toPlainText());
  });
}

void SettingsDialog::openDatabaseFolder() const {
  const auto path = QString::fromStdString(mConfig.db_conf.value_.db_pth.toString());
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void SettingsDialog::createBackup() {
  const auto defaultName =
      QStringLiteral("PsyClientManager-%1.psybackup")
          .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss"));
  const auto defaultDir =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const auto destinationPath = QFileDialog::getSaveFileName(
      this, tr("Create Backup"), QDir(defaultDir).filePath(defaultName),
      tr("PsyClientManager Backup (*.psybackup)"));
  if (destinationPath.isEmpty()) {
    return;
  }

  mCreateBackupButton->setEnabled(false);
  mValidateBackupButton->setEnabled(false);
  mBackupStatusLabel->setText(tr("Creating backup…"));
  mBackupStatusLabel->setVisible(true);
  mBackupProgressBar->setVisible(true);

  auto *thread = new QThread(this);
  auto *worker = new BackupWorker(mDb, destinationPath,
                                  pcm::app_settings::attachmentsStorageRoot());
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &BackupWorker::run);
  connect(worker, &BackupWorker::finished, this,
          [this, destinationPath](const bool ok, const QString &error) {
            mBackupStatusLabel->setVisible(false);
            mBackupProgressBar->setVisible(false);
            mCreateBackupButton->setEnabled(true);
            mValidateBackupButton->setEnabled(true);
            if (ok) {
              QMessageBox::information(
                  this, tr("Backup Created"),
                  tr("Backup created at:\n%1").arg(destinationPath));
            } else {
              QMessageBox::warning(this, tr("Backup Failed"), error);
            }
          });
  connect(worker, &BackupWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}

void SettingsDialog::browseAutoBackupDestination() {
  const auto selected = QFileDialog::getExistingDirectory(
      this, tr("Select Automatic Backup Folder"),
      mAutoBackupDestinationEdit->text());
  if (selected.isEmpty()) {
    return;
  }
  mAutoBackupDestinationEdit->setText(selected);
  pcm::app_settings::setAutoBackupDestination(selected);
}

void SettingsDialog::validateBackup() {
  const auto defaultDir =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const auto backupPath = QFileDialog::getOpenFileName(
      this, tr("Validate Backup"), defaultDir,
      tr("PsyClientManager Backup (*.psybackup)"));
  if (backupPath.isEmpty()) {
    return;
  }

  mCreateBackupButton->setEnabled(false);
  mValidateBackupButton->setEnabled(false);
  mBackupStatusLabel->setText(tr("Validating backup…"));
  mBackupStatusLabel->setVisible(true);
  mBackupProgressBar->setVisible(true);

  auto *thread = new QThread(this);
  auto *worker = new ValidateWorker(backupPath);
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &ValidateWorker::run);
  connect(worker, &ValidateWorker::finished, this,
          [this](const bool ok, const QStringList &errors) {
            mBackupStatusLabel->setVisible(false);
            mBackupProgressBar->setVisible(false);
            mCreateBackupButton->setEnabled(true);
            mValidateBackupButton->setEnabled(true);
            if (ok) {
              QMessageBox::information(this, tr("Backup Valid"),
                                       tr("The backup is valid."));
            } else {
              QString message;
              if (errors.size() > 10) {
                message = errors.mid(0, 10).join('\n') +
                          tr("\n... and %1 more").arg(errors.size() - 10);
              } else {
                message = errors.join('\n');
              }
              QMessageBox::warning(this, tr("Backup Invalid"), message);
            }
          });
  connect(worker, &ValidateWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}

void SettingsDialog::restoreBackup() {
  const auto defaultDir =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const auto backupPath = QFileDialog::getOpenFileName(
      this, tr("Restore Backup"), defaultDir,
      tr("PsyClientManager Backup (*.psybackup)"));
  if (backupPath.isEmpty()) {
    return;
  }

  mCreateBackupButton->setEnabled(false);
  mValidateBackupButton->setEnabled(false);
  mRestoreBackupButton->setEnabled(false);
  mBackupStatusLabel->setText(tr("Checking backup…"));
  mBackupStatusLabel->setVisible(true);
  mBackupProgressBar->setVisible(true);

  auto *thread = new QThread(this);
  auto *worker = new ValidateWorker(backupPath);
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &ValidateWorker::run);
  connect(worker, &ValidateWorker::finished, this,
          [this, backupPath](const bool ok, const QStringList &errors) {
            mBackupStatusLabel->setVisible(false);
            mBackupProgressBar->setVisible(false);
            mCreateBackupButton->setEnabled(true);
            mValidateBackupButton->setEnabled(true);
            mRestoreBackupButton->setEnabled(true);
            if (!ok) {
              QString message;
              if (errors.size() > 10) {
                message = errors.mid(0, 10).join('\n') +
                          tr("\n... and %1 more").arg(errors.size() - 10);
              } else {
                message = errors.join('\n');
              }
              QMessageBox::warning(this, tr("Backup Invalid"), message);
              return;
            }

            const auto confirmation = QMessageBox::warning(
                this, tr("Restore Backup"),
                tr("This will replace all current data (clients, events, "
                   "notes, and attachments) with the contents of this "
                   "backup. Your current data will be kept as a protective "
                   "copy, but the application must restart to complete the "
                   "restore. Continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (confirmation != QMessageBox::Yes) {
              return;
            }

            const auto markerPath = Poco::Path(mConfig.config_pth.value())
                                        .makeParent()
                                        .append("pending-restore.json")
                                        .toString();
            if (!pcm::backup::write_pending_restore_marker(
                    markerPath, backupPath.toStdString())) {
              QMessageBox::warning(
                  this, tr("Restore Failed"),
                  tr("Could not stage the restore. Check that there is "
                     "enough disk space and try again."));
              return;
            }

            QMessageBox::information(
                this, tr("Restore Staged"),
                tr("PsyClientManager will now close. Restart it to "
                   "complete the restore."));
            QApplication::quit();
          });
  connect(worker, &ValidateWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}

#include "settings_dialog.moc"
