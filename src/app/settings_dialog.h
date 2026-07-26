#pragma once

#include "config.h"

#include <QDialog>

#include <memory>

namespace pcm::database {
class Database;
}

class QDialogButtonBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTextEdit;
class QTimeEdit;

namespace oclero::qlementine {
class ColorEditor;
class SegmentedControl;
class Switch;
}

class SettingsDialog final : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(std::shared_ptr<pcm::database::Database> db,
                          QWidget *parent = nullptr);
  ~SettingsDialog() override = default;

private:
  void setupUi();
  void loadSettings() const;
  void connectSignals() const;
  void openDatabaseFolder() const;
  void createBackup();
  void validateBackup();

  oclero::qlementine::SegmentedControl *mSettingsSections{nullptr};
  QStackedWidget *mSettingsStack{nullptr};
  QComboBox *mLanguageCombo{nullptr};
  QLabel *mDatabasePathLabel{nullptr};
  QPushButton *mOpenDatabaseFolderButton{nullptr};
  QPushButton *mCreateBackupButton{nullptr};
  QPushButton *mValidateBackupButton{nullptr};
  QProgressBar *mBackupProgressBar{nullptr};
  QLabel *mBackupStatusLabel{nullptr};
  oclero::qlementine::Switch *mNotificationsEnabledSwitch{nullptr};
  QSpinBox *mNotificationLeadMinutesSpinBox{nullptr};
  oclero::qlementine::Switch *mPreventOverlapsSwitch{nullptr};
  oclero::qlementine::ColorEditor *mWorkEventColorEditor{nullptr};
  oclero::qlementine::ColorEditor *mPersonalEventColorEditor{nullptr};
  QComboBox *mCurrencyCombo{nullptr};
  QDoubleSpinBox *mDefaultWorkCostSpinBox{nullptr};
  QTimeEdit *mWorkDayStartEdit{nullptr};
  QTimeEdit *mWorkDayEndEdit{nullptr};
  QSpinBox *mDefaultSessionDurationSpinBox{nullptr};
  QSpinBox *mDefaultBufferBeforeSpinBox{nullptr};
  QSpinBox *mDefaultBufferAfterSpinBox{nullptr};
  QTextEdit *mMeetingInviteTemplateEdit{nullptr};
  QDialogButtonBox *mButtonBox{nullptr};
  pcm::config::Config mConfig;
  std::shared_ptr<pcm::database::Database> mDb;
};
