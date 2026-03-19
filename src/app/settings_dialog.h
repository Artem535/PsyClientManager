#pragma once

#include "config.h"

#include <QDialog>

class QDialogButtonBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimeEdit;

namespace oclero::qlementine {
class ColorEditor;
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

  QComboBox *mLanguageCombo{nullptr};
  QLabel *mDatabasePathLabel{nullptr};
  QPushButton *mOpenDatabaseFolderButton{nullptr};
  oclero::qlementine::Switch *mPreventOverlapsSwitch{nullptr};
  oclero::qlementine::ColorEditor *mWorkEventColorEditor{nullptr};
  oclero::qlementine::ColorEditor *mPersonalEventColorEditor{nullptr};
  QDoubleSpinBox *mDefaultWorkCostSpinBox{nullptr};
  QTimeEdit *mWorkDayStartEdit{nullptr};
  QTimeEdit *mWorkDayEndEdit{nullptr};
  QSpinBox *mDefaultSessionDurationSpinBox{nullptr};
  QDialogButtonBox *mButtonBox{nullptr};
  pcm::config::Config mConfig;
};
