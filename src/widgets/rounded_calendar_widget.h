#pragma once

#include <QCalendarWidget>
#include <QDate>
#include <QPaintEvent>
#include <QTextCharFormat>
#include <QWidget>

class RoundedCalendarWidget final : public QWidget {
  Q_OBJECT

public:
  explicit RoundedCalendarWidget(QWidget *parent = nullptr);

  void setSelectedDate(const QDate &date);
  [[nodiscard]] QDate selectedDate() const;
  void setDateTextFormat(const QDate &date, const QTextCharFormat &format);

signals:
  void clicked(const QDate &date);

private:
  void paintEvent(QPaintEvent *event) override;

  QCalendarWidget *mCalendarWidget = nullptr;
};
