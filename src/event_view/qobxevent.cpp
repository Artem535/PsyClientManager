//
// Created by a.durynin on 05.09.2025.
//

#include "qobxevent.h"

// Default constructor
QObxEvent::QObxEvent()
    : mId(0), mIsWorkEvent(false), mEventStatId(0), mPaymentStatId(0),
      mDuration(0) {}

// Constructor with all fields
QObxEvent::QObxEvent(obx_id id, const QString &name,
                     const QString &description, bool isWorkEvent,
                     obx_id eventStatId, obx_id paymentStatId,
                     const QDateTime &startDate, const QDateTime &endDate,
                     qint64 duration)
    : mId(id), mName(name), mDescription(description),
      mIsWorkEvent(isWorkEvent), mEventStatId(eventStatId),
      mPaymentStatId(paymentStatId), mStartDate(startDate), mEndDate(endDate),
      mDuration(duration) {}

// Constructor from ObxEvent
QObxEvent::QObxEvent(const ObxEvent &obxEvent)
    : mId(obxEvent.id), mName(QString::fromStdString(obxEvent.name)),
      mDescription(QString::fromStdString(obxEvent.description)),
      mIsWorkEvent(obxEvent.is_work_event),
      mEventStatId(obxEvent.event_stat_id),
      mPaymentStatId(obxEvent.payment_stat_id),
      mStartDate(QDateTime::fromSecsSinceEpoch(obxEvent.start_date)),
      mEndDate(QDateTime::fromSecsSinceEpoch(obxEvent.end_date)),
      mDuration(obxEvent.duration) {}

// Convert back to ObxEvent
ObxEvent QObxEvent::toObxEvent() const {
  ObxEvent obxEvent{};
  obxEvent.id = mId;
  obxEvent.name = mName.toStdString();
  obxEvent.description = mDescription.toStdString();
  obxEvent.is_work_event = mIsWorkEvent;
  obxEvent.event_stat_id = mEventStatId;
  obxEvent.payment_stat_id = mPaymentStatId;
  obxEvent.start_date = mStartDate.toSecsSinceEpoch();
  obxEvent.end_date = mEndDate.toSecsSinceEpoch();
  obxEvent.duration = mDuration;
  return obxEvent;
}

// Getters
obx_id QObxEvent::getId() const { return mId; }
QString QObxEvent::getName() const { return mName; }
QString QObxEvent::getDescription() const { return mDescription; }
bool QObxEvent::getIsWorkEvent() const { return mIsWorkEvent; }
obx_id QObxEvent::getEventStatId() const { return mEventStatId; }
obx_id QObxEvent::getPaymentStatId() const { return mPaymentStatId; }
QDateTime QObxEvent::getStartDate() const { return mStartDate; }
QDateTime QObxEvent::getEndDate() const { return mEndDate; }
qint64 QObxEvent::getDuration() const { return mDuration; }

// Setters
void QObxEvent::setId(obx_id id) { mId = id; }
void QObxEvent::setName(const QString &name) { mName = name; }
void QObxEvent::setDescription(const QString &description) {
  mDescription = description;
}
void QObxEvent::setIsWorkEvent(bool isWorkEvent) { mIsWorkEvent = isWorkEvent; }
void QObxEvent::setEventStatId(obx_id eventStatId) {
  mEventStatId = eventStatId;
}
void QObxEvent::setPaymentStatId(obx_id paymentStatId) {
  mPaymentStatId = paymentStatId;
}
void QObxEvent::setStartDate(const QDateTime &startDate) {
  mStartDate = startDate;
}
void QObxEvent::setEndDate(const QDateTime &endDate) { mEndDate = endDate; }
void QObxEvent::setDuration(qint64 duration) { mDuration = duration; }
