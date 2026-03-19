#pragma once

#include <QDate>
#include <QDateTime>
#include <QPair>
#include <QLabel>
#include <QGridLayout>
#include <QTime>
#include <QVector>
#include <QWidget>

class QuickSlotsWidget final : public QWidget {
  Q_OBJECT

public:
  explicit QuickSlotsWidget(QWidget *parent = nullptr);

  void setSelectedDate(const QDate &date);
  void setBusyIntervals(const QVector<QPair<QDateTime, QDateTime>> &intervals);

public slots:
  void refreshSlots();

signals:
  void quickSlotSelected(const QTime &startTime, int durationMinutes);

private:
  [[nodiscard]] bool slotOverlapsAnyEvent(const QDateTime &slotStart,
                                          const QDateTime &slotEnd) const;
  void clearSlots();

  QLabel *mTitleLabel = nullptr;
  QWidget *mSlotsContainer = nullptr;
  QGridLayout *mSlotsLayout = nullptr;
  QLabel *mEmptyLabel = nullptr;

  QDate mSelectedDate;
  QVector<QPair<QDateTime, QDateTime>> mBusyIntervals;
};
