//
// Created by a.durynin on 31.08.2025.
//

#include "qclient.h"

#include <QTimeZone>

QClient::QClient() {}

// ---- Constructors ----
QClient::QClient(const ObxClient &client) { update(client); }

// ---- Update from database ----
void QClient::update(const ObxClient &client) {
  setId(client.id);
  setName(QString::fromStdString(client.name != std::nullopt ? client.name.value() : ""));
  setLastName(QString::fromStdString(client.last_name != std::nullopt ? client.last_name.value() : ""));
  setPhoneNumber(QString::fromStdString(client.phone_number != std::nullopt ? client.phone_number.value() : ""));
  setEmail(QString::fromStdString(client.email != std::nullopt ? client.email.value() : ""));
  setBirthDate(client.birthday_date != std::nullopt ? QDateTime::fromMSecsSinceEpoch(client.birthday_date.value()).date() : QDate());
  setCountry(QString::fromStdString(client.country != std::nullopt ? client.country.value() : ""));
  setCity(QString::fromStdString(client.city != std::nullopt ? client.city.value() : ""));
  setTimezone(QString::fromStdString(client.time_zone != std::nullopt ? client.time_zone.value() : ""));
  setAdditionalInfo(QString::fromStdString(client.additional_info != std::nullopt ? client.additional_info.value() : ""));
  setDiagnosis(QString::fromStdString(client.diagnosis != std::nullopt ? client.diagnosis.value() : ""));
}

void QClient::clear() {
  mId = 0;
  mName.clear();
  mLastName.clear();
  mPhoneNumber.clear();
  mEmail.clear();
  mBirthDate = {};
  mAge.clear();
  mCountry.clear();
  mCity.clear();
  mTimezone.clear();
  mAdditionalInfo.clear();
  mDiagnosis.clear();
}

// ---- Getters ----
obx_id QClient::getId() const { return mId; }
QString QClient::getName() const { return mName; }
QString QClient::getLastName() const { return mLastName; }
QString QClient::getPhoneNumber() const { return mPhoneNumber; }
QString QClient::getEmail() const { return mEmail; }
QDate QClient::getBirthDate() const { return mBirthDate; }
QString QClient::getAge() const { return mAge; }
QString QClient::getCountry() const { return mCountry; }
QString QClient::getCity() const { return mCity; }
QString QClient::getTimezone() const { return mTimezone; }
QString QClient::getAdditionalInfo() const { return mAdditionalInfo; }
QString QClient::getDiagnosis() const { return mDiagnosis; }
bool QClient::isActive() const { return mIsActive; }

// ---- Setters ----
void QClient::setName(const QString &name) { mName = name; }
void QClient::setLastName(const QString &lastName) { mLastName = lastName; }
void QClient::setPhoneNumber(const QString &phoneNumber) {
  mPhoneNumber = phoneNumber;
}
void QClient::setEmail(const QString &email) { mEmail = email; }

void QClient::setBirthDate(const QDate &birthDate) {
  mBirthDate = birthDate;
  mAge = QString::number(countAge(birthDate));
}

void QClient::setAge(const QString &age) { mAge = age; }
void QClient::setCountry(const QString &country) { mCountry = country; }
void QClient::setCity(const QString &city) { mCity = city; }
void QClient::setTimezone(const QString &timezone) { mTimezone = timezone; }
void QClient::setAdditionalInfo(const QString &info) { mAdditionalInfo = info; }
void QClient::setDiagnosis(const QString &diagnosis) { mDiagnosis = diagnosis; }

void QClient::setId(const obx_id id) { mId = id; }
void QClient::setIsActive(const bool isActive) { mIsActive = isActive; }

// ---- Convert to database entity ----
ObxClient QClient::toObxClient() const {
  ObxClient client;

  // Basic info
  client.id = getId();
  client.name = getName().toStdString();
  client.last_name = getLastName().toStdString();
  client.phone_number = getPhoneNumber().toStdString();
  client.email = getEmail().toStdString();

  // Birthdate stored as UTC seconds since epoch
  client.birthday_date = QDateTime(getBirthDate(), QTime(12, 0), QTimeZone::UTC)
                             .toSecsSinceEpoch();

  // Location info
  client.country = getCountry().toStdString();
  client.city = getCity().toStdString();
  client.time_zone = getTimezone().toStdString();

  // Additional info
  client.additional_info = getAdditionalInfo().toStdString();
  client.diagnosis = getDiagnosis().toStdString();

  client.client_active = mIsActive;

  return client;
}

// ---- Helpers ----
int QClient::countAge(const QDate &birthDate) {
  if (!birthDate.isValid())
    return 0;
  const QDate today = QDate::currentDate();
  int years = today.year() - birthDate.year();
  if (today.month() < birthDate.month() ||
      (today.month() == birthDate.month() && today.day() < birthDate.day())) {
    years--;
  }
  return years;
}
