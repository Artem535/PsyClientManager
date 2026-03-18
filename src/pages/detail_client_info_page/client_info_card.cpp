#include "client_info_card.h"
#include "ui/pages/ui_detailclientinfo.h"

#include <oclero/qlementine/widgets/Switch.hpp>

#include <QIcon>
#include <QSize>

Q_LOGGING_CATEGORY(logClientInfoCard, "pcm.ClientInfoCard");

QClientInfoCardPage::QClientInfoCardPage(QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::ClientInfoCard>()) {
  mUi->setupUi(this);

  mIsActiveSwitch = new oclero::qlementine::Switch(this);
  mIsActiveSwitch->setText(mUi->isActive->text());
  mUi->gridLayout->replaceWidget(mUi->isActive, mIsActiveSwitch);
  mUi->isActive->hide();
  mUi->isActive->deleteLater();

  mUi->editButton->setIcon(QIcon(":/icons/user-pen-solid-full.svg"));
  mUi->editButton->setIconSize(QSize(16, 16));

  connectReactiveSignals();
  connectSignals();

  initDefaultStyle();
}

QClientInfoCardPage::~QClientInfoCardPage() = default;

void QClientInfoCardPage::setClientInfo(
    const std::optional<ObxClient> &client) {
  if (client.has_value()) {
    mClientInfo.update(client.value());
  } else {
    mClientInfo.clear();
  }

  emit provideClearUI();
  emit provideUpdateUI();
}
void QClientInfoCardPage::enterInEditMode() {
  setReadOnly(false);
  emit provideEditModeStyle();
}
void QClientInfoCardPage::cancelEditMode() {
  setReadOnly(true);
  emit provideDefaultStyle();
  emit provideUpdateUI();
}

void QClientInfoCardPage::leaveEditMode() {
  QClient tmpClient;
  tmpClient.setId(mClientInfo.getId());

  tmpClient.setName(mUi->nameInput->text());
  tmpClient.setLastName(mUi->lastNameInput->text());
  tmpClient.setPhoneNumber(mUi->phoneInput->text());
  tmpClient.setEmail(mUi->emailInput->text());
  tmpClient.setBirthDate(mUi->birthdateInput->date());
  tmpClient.setCountry(mUi->countryInput->text());
  tmpClient.setCity(mUi->cityInput->text());
  tmpClient.setTimezone(mUi->timezoneInput->text());
  tmpClient.setDiagnosis(mUi->diagnosisTextEdit->toPlainText());
  tmpClient.setAdditionalInfo(mUi->additionalInfoTextEdit->toPlainText());
  tmpClient.setIsActive(mIsActiveSwitch->isChecked());

  if (tmpClient.getName().isEmpty() || tmpClient.getLastName().isEmpty()) {
    QMessageBox::warning(this, tr(": WARNING_TITLE"),
                         tr(": CLIENT_NAME_AND_LAST_NAME_REQUIRED"));
    return;
  }

  mClientInfo = std::move(tmpClient);

  emit provideDefaultStyle();
  emit provideUpdateUI();
  emit endEditMode();
  emit provideSaveClient(mClientInfo.toObxClient());
}

void QClientInfoCardPage::updateUiProperty() const {
  QString initials;
  if (!mClientInfo.getName().isEmpty())
    initials.append(mClientInfo.getName().at(0).toUpper());
  if (!mClientInfo.getLastName().isEmpty())
    initials.append(mClientInfo.getLastName().at(0).toUpper());
  mUi->avatarLabel->setText(initials.isEmpty() ? "??" : initials);

  mUi->nameInput->setText(mClientInfo.getName());
  mUi->lastNameInput->setText(mClientInfo.getLastName());
  mUi->phoneInput->setText(mClientInfo.getPhoneNumber());
  mUi->emailInput->setText(mClientInfo.getEmail());

  const auto date = mClientInfo.getBirthDate().isValid()
                        ? mClientInfo.getBirthDate()
                        : QDate::currentDate();
  mUi->birthdateInput->setDate(date);
  mUi->ageInput->setText(mClientInfo.getAge());

  mUi->countryInput->setText(mClientInfo.getCountry());
  mUi->cityInput->setText(mClientInfo.getCity());
  mUi->timezoneInput->setText(mClientInfo.getTimezone());
  mIsActiveSwitch->setChecked(mClientInfo.isActive());

  mUi->diagnosisTextEdit->setPlainText(mClientInfo.getDiagnosis());
  mUi->additionalInfoTextEdit->setPlainText(mClientInfo.getAdditionalInfo());
}

void QClientInfoCardPage::connectReactiveSignals() {
  // Updating age with date changed
  connect(mUi->birthdateInput, &QDateTimeEdit::dateChanged, this,
          &QClientInfoCardPage::updateAgeInput);

  // Update Avatar when name and last name entered
  connect(mUi->nameInput, &QLineEdit::textChanged, this,
          &QClientInfoCardPage::updateAvatar);
  connect(mUi->lastNameInput, &QLineEdit::textChanged, this,
          &QClientInfoCardPage::updateAvatar);

  // Update text in isActive checkbox
  // clang-format off
  connect(mIsActiveSwitch, &QAbstractButton::toggled, [this](const auto checked) {
    const auto text = checked ? tr(": CLIENT_STATUS_ACTIVE")
                              : tr(": CLIENT_STATUS_INACTIVE");
    mIsActiveSwitch->setText(QString(text));
  });
  // clang-format on
}

void QClientInfoCardPage::connectSignals() const {
  connect(this, &QClientInfoCardPage::provideUpdateUI, this,
          &QClientInfoCardPage::updateUiProperty);
  connect(this, &QClientInfoCardPage::provideClearUI, this,
          &QClientInfoCardPage::clearUI);
  connect(this, &QClientInfoCardPage::provideEditModeStyle, this,
          &QClientInfoCardPage::initEditModeStyle);
  connect(this, &QClientInfoCardPage::provideDefaultStyle, this,
          &QClientInfoCardPage::initDefaultStyle);

  // Button "Edit"
  connect(mUi->editButton, &QPushButton::clicked, this,
          &QClientInfoCardPage::enterInEditMode);
  // Button "Save"
  connect(mUi->buttonBox, &QDialogButtonBox::accepted, this,
          &QClientInfoCardPage::leaveEditMode);
  // Button "Cancel"
  connect(mUi->buttonBox, &QDialogButtonBox::rejected, this,
          &QClientInfoCardPage::cancelEditMode);

}
void QClientInfoCardPage::setReadOnly(const bool readOnly) const {
  mUi->nameInput->setReadOnly(readOnly);
  mUi->lastNameInput->setReadOnly(readOnly);
  mUi->phoneInput->setReadOnly(readOnly);
  mUi->emailInput->setReadOnly(readOnly);
  mUi->birthdateInput->setReadOnly(readOnly);
  mUi->countryInput->setReadOnly(readOnly);
  mUi->cityInput->setReadOnly(readOnly);
  mUi->timezoneInput->setReadOnly(readOnly);
  mUi->diagnosisTextEdit->setReadOnly(readOnly);
  mUi->additionalInfoTextEdit->setReadOnly(readOnly);
}

void QClientInfoCardPage::initDefaultStyle() {
  mUi->buttonBox->setVisible(false);
  mUi->editButton->setVisible(true);
  setReadOnly(true);
  emit provideClearUI();
}

void QClientInfoCardPage::initEditModeStyle() const {
  mUi->buttonBox->setVisible(true);
  mUi->editButton->setVisible(false);
}

void QClientInfoCardPage::updateAgeInput(const QDate &date) const {
  const int age{countAge(date)};
  const auto ageStr = QString::number(age);
  mUi->ageInput->setText(ageStr);
}

void QClientInfoCardPage::updateAvatar() const {
  QString initials;

  if (const auto text = mUi->nameInput->text(); !text.isEmpty()) {
    const auto firstLatter = text[0];
    initials += firstLatter.toUpper();
  }

  if (const auto text = mUi->lastNameInput->text(); !text.isEmpty()) {
    const auto firstLatter = text[0];
    initials += firstLatter.toUpper();
  }

  mUi->avatarLabel->setText(initials.isEmpty() ? "??" : initials);
}

int QClientInfoCardPage::countAge(const QDate &birthDate) {
  const QDate now = QDate::currentDate();
  int age = now.year() - birthDate.year();
  if (now.month() < birthDate.month() ||
      (now.month() == birthDate.month() && now.day() < birthDate.day())) {
    age--;
  }
  return age;
}

void QClientInfoCardPage::clearUI() const {
  mUi->nameInput->clear();
  mUi->lastNameInput->clear();
  mUi->phoneInput->clear();
  mUi->emailInput->clear();
  mUi->birthdateInput->setDate(QDate::currentDate());
  mUi->ageInput->clear();
  mUi->countryInput->clear();
  mUi->cityInput->clear();
  mUi->timezoneInput->clear();
  mUi->diagnosisTextEdit->clear();
  mUi->additionalInfoTextEdit->clear();
  mUi->avatarLabel->setText("??");
  mIsActiveSwitch->setChecked(false);
}
