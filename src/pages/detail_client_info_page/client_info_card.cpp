#include "client_info_card.h"
#include "ui/pages/ui_detailclientinfo.h"

Q_LOGGING_CATEGORY(logClientInfoCard, "pcm.ClientInfoCard");

QClientInfoCardPage::QClientInfoCardPage(QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::ClientInfoCard>()) {
  mUi->setupUi(this);
}

void QClientInfoCardPage::setClientInfo(
    const std::optional<ObxClient> &client) {
  if (client.has_value()) {
    mClientInfo.update(client.value());
  } else {
    mClientInfo.clear();
  }

  emit clearUI();
  emit provideUpdateUI();
}
void QClientInfoCardPage::updateUiProperty() {
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

  mUi->birthdateInput->setDate(
      mClientInfo.getBirthDate().isValid()
          ? mClientInfo.getBirthDate()
          : QDate::currentDate());
  mUi->ageInput->setText(mClientInfo.getAge());

  mUi->countryInput->setText(mClientInfo.getCountry());
  mUi->cityInput->setText(mClientInfo.getCity());
  mUi->timezoneInput->setText(mClientInfo.getTimezone());

  mUi->diagnosisTextEdit->setPlainText(mClientInfo.getDiagnosis());
  mUi->additionalInfoTextEdit->setPlainText(mClientInfo.getAdditionalInfo());
}

QClientInfoCardPage::~QClientInfoCardPage() = default;

int QClientInfoCardPage::countAge(const QDate &birthDate) {
  const QDate now = QDate::currentDate();
  int age = now.year() - birthDate.year();
  if (now.month() < birthDate.month() ||
      (now.month() == birthDate.month() && now.day() < birthDate.day())) {
    age--;
  }
  return age;
}
