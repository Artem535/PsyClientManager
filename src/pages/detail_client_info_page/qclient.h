//
// Created by a.durynin on 31.08.2025.
//

#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

#include "database.h"

class QClient {
public:
  QClient();
  explicit QClient(const DuckClient &client);

  void update(const DuckClient &client);
  void clear();

  // ---- Getters ----
  int64_t getId() const;
  QString getName() const;
  QString getLastName() const;
  QString getPhoneNumber() const;
  QString getEmail() const;
  QDate getBirthDate() const;
  QString getAge() const;
  QString getCountry() const;
  QString getCity() const;
  QString getTimezone() const;
  QString getAdditionalInfo() const;
  QString getDiagnosis() const;
  bool isActive() const;

  // ---- Setters ----
  void setName(const QString& name);
  void setLastName(const QString& lastName);
  void setPhoneNumber(const QString& phoneNumber);
  void setEmail(const QString& email);
  void setBirthDate(const QDate& birthDate);
  void setAge(const QString& age);
  void setCountry(const QString& country);
  void setCity(const QString& city);
  void setTimezone(const QString& timezone);
  void setAdditionalInfo(const QString& info);
  void setDiagnosis(const QString& diagnosis);
  void setId(const int64_t id);
  void setIsActive(const bool isActive);

  // ---- Others ----
  [[nodiscard]] DuckClient toDuckClient() const;

private:
  int64_t mId = 0;
  QString mName;
  QString mLastName;
  QString mPhoneNumber;
  QString mEmail;
  QDate mBirthDate;
  QString mAge;
  QString mCountry;
  QString mCity;
  QString mTimezone;
  QString mAdditionalInfo;
  QString mDiagnosis;
  bool mIsActive = false;

  static int countAge(const QDate &birthDate);
};
