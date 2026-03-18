#include "rounded_calendar_widget.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QTableView>

#include "constants.hpp"

RoundedCalendarWidget::RoundedCalendarWidget(QWidget *parent) : QWidget(parent) {
  setObjectName("roundedCalendarCard");
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  mCalendarWidget = new QCalendarWidget(this);
  mCalendarWidget->setObjectName("innerCalendarWidget");
  mCalendarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  mCalendarWidget->setMaximumSize(500, 262);
  mCalendarWidget->setAutoFillBackground(false);
  mCalendarWidget->setContentsMargins(0, 0, 0, 0);

  layout->addWidget(mCalendarWidget);

  setStyleSheet(
      "QCalendarWidget#innerCalendarWidget {"
      " border: none;"
      " background: transparent;"
      "}"
      "QCalendarWidget#innerCalendarWidget QWidget {"
      " background: transparent;"
      "}"
      "QCalendarWidget#innerCalendarWidget QWidget#qt_calendar_navigationbar {"
      " border: none;"
      " background: transparent;"
      "}"
      "QCalendarWidget#innerCalendarWidget QToolButton {"
      " background: transparent;"
      "}"
      "QCalendarWidget#innerCalendarWidget QTableView {"
      " background: transparent;"
      " border: none;"
      " outline: 0;"
      "}"
      "QCalendarWidget#innerCalendarWidget QAbstractItemView:enabled {"
      " background: transparent;"
      " border: none;"
      " outline: 0;"
      " selection-background-color: transparent;"
      "}"
      "QCalendarWidget#innerCalendarWidget QAbstractScrollArea {"
      " background: transparent;"
      " border: none;"
      "}"
      "QCalendarWidget#innerCalendarWidget QAbstractScrollArea > QWidget {"
      " background: transparent;"
      "}");

  if (auto *tableView = mCalendarWidget->findChild<QTableView *>()) {
    tableView->setFrameShape(QFrame::NoFrame);
    tableView->setStyleSheet(
        "QTableView { background: transparent; border: none; }"
        "QTableView::item { background: transparent; }");
    if (auto *viewport = tableView->viewport()) {
      viewport->setAutoFillBackground(false);
      viewport->setStyleSheet("background: transparent;");
    }
  }

  connect(mCalendarWidget, &QCalendarWidget::clicked, this,
          &RoundedCalendarWidget::clicked);
}

void RoundedCalendarWidget::setSelectedDate(const QDate &date) {
  mCalendarWidget->setSelectedDate(date);
}

QDate RoundedCalendarWidget::selectedDate() const {
  return mCalendarWidget->selectedDate();
}

void RoundedCalendarWidget::setDateTextFormat(const QDate &date,
                                              const QTextCharFormat &format) {
  mCalendarWidget->setDateTextFormat(date, format);
}

void RoundedCalendarWidget::paintEvent(QPaintEvent *event) {
  QWidget::paintEvent(event);

  constexpr qreal kRadius = 16.0;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(pcm::widgets::constants::kCalendarCardBackgroundColor);
  painter.drawRoundedRect(rect(), kRadius, kRadius);
}
