#include "client_info_card.h"
#include "ui/pages/ui_detailclientinfo.h"

Q_LOGGING_CATEGORY(logClientInfoCard, "pcm.ClientInfoCard");

QClientInfoCardPage::QClientInfoCardPage(QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::ClientInfoCard>()) {
    mUi->setupUi(this);

    connectReactiveSignals();
    connectSignals();

    emit provideUpdateUI();
}

QClientInfoCardPage::~QClientInfoCardPage() = default;

void QClientInfoCardPage::setClientInfo(const std::optional<ObxClient> &client) {
    if (client.has_value()) {
        mClientInfo.update(client.value());
    } else {
        mClientInfo.clear();
    }

    emit provideClearUI();
    emit provideUpdateUI();
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

    mUi->diagnosisTextEdit->setPlainText(mClientInfo.getDiagnosis());
    mUi->additionalInfoTextEdit->setPlainText(mClientInfo.getAdditionalInfo());
}

void QClientInfoCardPage::connectReactiveSignals() {
    // Updating age with date changed
    connect(mUi->birthdateInput, &QDateTimeEdit::dateChanged, this, &QClientInfoCardPage::updateAgeInput);

    // Update Avatar when name and last name entered
    connect(mUi->nameInput, &QLineEdit::textChanged, this, &QClientInfoCardPage::updateAvatar);
    connect(mUi->lastNameInput, &QLineEdit::textChanged, this, &QClientInfoCardPage::updateAvatar);
}

void QClientInfoCardPage::connectSignals() {
    connect(this, &QClientInfoCardPage::provideUpdateUI, this, &QClientInfoCardPage::updateUiProperty);
    connect(this, &QClientInfoCardPage::provideClearUI, this, &QClientInfoCardPage::clearUI);
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
}
