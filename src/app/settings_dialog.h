#pragma once

#include "config.h"

#include <QDialog>

class QDialogButtonBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
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
  explicit SettingsDialog(QWidget *parent = nullptr);
  ~SettingsDialog() override = default;

private:
  void setupUi();
  void loadSettings() const;
  void connectSignals() const;
  void openDatabaseFolder() const;

  oclero::qlementine::SegmentedControl *mSettingsSections{nullptr};
  QStackedWidget *mSettingsStack{nullptr};
  QComboBox *mLanguageCombo{nullptr};
  QLabel *mDatabasePathLabel{nullptr};
  QPushButton *mOpenDatabaseFolderButton{nullptr};
  oclero::qlementine::Switch *mNotificationsEnabledSwitch{nullptr};
  QSpinBox *mNotificationLeadMinutesSpinBox{nullptr};
  oclero::qlementine::Switch *mPreventOverlapsSwitch{nullptr};
  oclero::qlementine::ColorEditor *mWorkEventColorEditor{nullptr};
  oclero::qlementine::ColorEditor *mPersonalEventColorEditor{nullptr};
  QDoubleSpinBox *mDefaultWorkCostSpinBox{nullptr};
  QTimeEdit *mWorkDayStartEdit{nullptr};
  QTimeEdit *mWorkDayEndEdit{nullptr};
  QSpinBox *mDefaultSessionDurationSpinBox{nullptr};
  QTextEdit *mMeetingInviteTemplateEdit{nullptr};
  QDialogButtonBox *mButtonBox{nullptr};
  pcm::config::Config mConfig;
};
