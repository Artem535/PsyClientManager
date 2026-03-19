#include "settings_dialog.h"

#include "../widgets/app_settings.h"

#include <oclero/qlementine/widgets/ColorEditor.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>

#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QUrl>
#include <QVBoxLayout>

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
} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent) {
  setupUi();
  loadSettings();
  connectSignals();
}

void SettingsDialog::setupUi() {
  setWindowTitle(tr("Settings"));
  setModal(true);
  resize(460, 260);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(20, 20, 20, 20);
  rootLayout->setSpacing(16);

  auto *caption = new QLabel(tr("Application settings"), this);
  QFont captionFont = caption->font();
  captionFont.setPointSize(captionFont.pointSize() + 1);
  captionFont.setBold(true);
  caption->setFont(captionFont);
  rootLayout->addWidget(caption);

  auto *languageBox = new QGroupBox(tr("Language"), this);
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
  rootLayout->addWidget(languageBox);

  auto *databaseBox = new QGroupBox(tr("Database"), this);
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
  rootLayout->addWidget(databaseBox);

  auto *eventsBox = new QGroupBox(tr("Timeline colors"), this);
  auto *eventsLayout = new QVBoxLayout(eventsBox);
  eventsLayout->setContentsMargins(16, 16, 16, 16);
  eventsLayout->setSpacing(14);
  mPreventOverlapsSwitch = new oclero::qlementine::Switch(eventsBox);
  mWorkEventColorEditor = new oclero::qlementine::ColorEditor(eventsBox);
  mPersonalEventColorEditor = new oclero::qlementine::ColorEditor(eventsBox);
  mDefaultWorkCostSpinBox = new QDoubleSpinBox(eventsBox);
  mDefaultWorkCostSpinBox->setDecimals(2);
  mDefaultWorkCostSpinBox->setMinimum(0.0);
  mDefaultWorkCostSpinBox->setMaximum(1'000'000.0);
  mDefaultWorkCostSpinBox->setSingleStep(100.0);
  mDefaultWorkCostSpinBox->setSuffix(tr(" ₽"));
  mWorkDayStartEdit = new QTimeEdit(eventsBox);
  mWorkDayStartEdit->setDisplayFormat("HH:mm");
  mWorkDayEndEdit = new QTimeEdit(eventsBox);
  mWorkDayEndEdit->setDisplayFormat("HH:mm");
  mDefaultSessionDurationSpinBox = new QSpinBox(eventsBox);
  mDefaultSessionDurationSpinBox->setMinimum(5);
  mDefaultSessionDurationSpinBox->setMaximum(480);
  mDefaultSessionDurationSpinBox->setSingleStep(5);
  mDefaultSessionDurationSpinBox->setSuffix(tr(" min"));
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
  rootLayout->addWidget(eventsBox);
  rootLayout->addStretch();

  mButtonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
  rootLayout->addWidget(mButtonBox);
}

void SettingsDialog::loadSettings() const {
  const auto languageCode = pcm::app_settings::languageCode();
  const auto languageIndex = mLanguageCombo->findData(languageCode);
  mLanguageCombo->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
  mDatabasePathLabel->setText(
      QString::fromStdString(mConfig.db_conf.value_.db_pth.toString()));
  mPreventOverlapsSwitch->setChecked(pcm::app_settings::preventEventOverlaps());
  mWorkDayStartEdit->setTime(pcm::app_settings::workDayStart());
  mWorkDayEndEdit->setTime(pcm::app_settings::workDayEnd());
  mDefaultSessionDurationSpinBox->setValue(
      pcm::app_settings::defaultSessionDurationMinutes());
  mDefaultWorkCostSpinBox->setValue(pcm::app_settings::defaultWorkEventCost());
  mWorkEventColorEditor->setColor(pcm::app_settings::workEventColor());
  mPersonalEventColorEditor->setColor(pcm::app_settings::personalEventColor());
}

void SettingsDialog::connectSignals() const {
  connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
  connect(mLanguageCombo, &QComboBox::currentIndexChanged, this, [this](const int index) {
    pcm::app_settings::setLanguageCode(mLanguageCombo->itemData(index).toString());
  });
  connect(mOpenDatabaseFolderButton, &QPushButton::clicked, this,
          &SettingsDialog::openDatabaseFolder);
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
}

void SettingsDialog::openDatabaseFolder() const {
  const auto path = QString::fromStdString(mConfig.db_conf.value_.db_pth.toString());
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
