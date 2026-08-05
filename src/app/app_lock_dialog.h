#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace pcm {
class AppLockService;

class AppLockDialog final : public QDialog {
  Q_OBJECT

public:
  explicit AppLockDialog(AppLockService &service, QWidget *parent = nullptr);

protected:
  void reject() override;

private:
  void unlock();

  AppLockService &mService;
  QLineEdit *mCredentialEdit{nullptr};
  QLabel *mErrorLabel{nullptr};
  QPushButton *mUnlockButton{nullptr};
};
} // namespace pcm
