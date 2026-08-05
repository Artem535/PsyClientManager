#include "app_lock_dialog.h"

#include "app_lock_service.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace pcm {

AppLockDialog::AppLockDialog(AppLockService &service, QWidget *parent)
    : QDialog(parent), mService(service) {
  setWindowTitle(tr("Unlock PsyClientManager"));
  setModal(true);
  setWindowModality(Qt::ApplicationModal);
  setWindowFlag(Qt::WindowCloseButtonHint, false);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setMinimumWidth(340);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(12);

  auto *title = new QLabel(tr("Application is locked"), this);
  QFont titleFont = title->font();
  titleFont.setBold(true);
  titleFont.setPointSize(titleFont.pointSize() + 2);
  title->setFont(titleFont);
  layout->addWidget(title);
  layout->addWidget(new QLabel(tr("Enter your PIN or password to continue."), this));

  mCredentialEdit = new QLineEdit(this);
  mCredentialEdit->setEchoMode(QLineEdit::Password);
  mCredentialEdit->setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText);
  layout->addWidget(mCredentialEdit);

  mErrorLabel = new QLabel(this);
  mErrorLabel->setStyleSheet("color: #ef7777;");
  mErrorLabel->setVisible(false);
  layout->addWidget(mErrorLabel);

  mUnlockButton = new QPushButton(tr("Unlock"), this);
  layout->addWidget(mUnlockButton, 0, Qt::AlignRight);
  connect(mUnlockButton, &QPushButton::clicked, this, &AppLockDialog::unlock);
  connect(mCredentialEdit, &QLineEdit::returnPressed, this, &AppLockDialog::unlock);
}

void AppLockDialog::reject() {}

void AppLockDialog::unlock() {
  auto credential = mCredentialEdit->text().toUtf8();
  const bool valid = mService.verify(
      std::string_view{credential.constData(), static_cast<std::size_t>(credential.size())});
  credential.fill('\0');
  if (!valid) {
    mCredentialEdit->clear();
    mErrorLabel->setText(tr("Incorrect PIN or password."));
    mErrorLabel->setVisible(true);
    return;
  }

  mCredentialEdit->clear();
  accept();
}

} // namespace pcm
