#pragma once

#include <QDateTime>
#include <QString>
#include  "database.h"

// This class is a Qt wrapper for the ObjectBox entity "ObxEvent".
// It provides getters, setters, and conversion constructors for easy use in Qt
// models.

class QObxEvent {
public:
  // Default constructor
  QObxEvent();

  // Constructor with all fields
  QObxEvent(obx_id id, const QString &name, const QString &description,
            bool isWorkEvent, obx_id eventStatId, obx_id paymentStatId,
            const QDateTime &startDate, const QDateTime &endDate,
            qint64 duration);

  // Constructor from ObxEvent (ObjectBox generated struct/class)
  explicit QObxEvent(const ObxEvent &obxEvent);

  // Convert back to ObxEvent
  ObxEvent toObxEvent() const;

  // Getters
  obx_id getId() const;
  QString getName() const;
  QString getDescription() const;
  bool getIsWorkEvent() const;
  obx_id getEventStatId() const;
  obx_id getPaymentStatId() const;
  QDateTime getStartDate() const;
  QDateTime getEndDate() const;
  qint64 getDuration() const;

  // Setters
  void setId(obx_id id);
  void setName(const QString &name);
  void setDescription(const QString &description);
  void setIsWorkEvent(bool isWorkEvent);
  void setEventStatId(obx_id eventStatId);
  void setPaymentStatId(obx_id paymentStatId);
  void setStartDate(const QDateTime &startDate);
  void setEndDate(const QDateTime &endDate);
  void setDuration(qint64 duration);

private:
  obx_id mId;
  QString mName;
  QString mDescription;
  bool mIsWorkEvent;
  obx_id mEventStatId;
  obx_id mPaymentStatId;
  QDateTime mStartDate;
  QDateTime mEndDate;
  qint64 mDuration;
};
