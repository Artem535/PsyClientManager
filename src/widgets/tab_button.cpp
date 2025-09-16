//
// Created by a.durynin on 10.09.2025.
//

#include "tab_button.h"

#include <QPainter>

TabButton::TabButton(const QString &text, QWidget *parent)
    : QPushButton(parent), mText(text) {
  setCheckable(true);
  setFlat(true); // removes native border
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

void TabButton::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Transparent background
  painter.setBrush(Qt::NoBrush);
  painter.setPen(Qt::NoPen);
  painter.drawRect(rect());

  constexpr auto circleDiameter = pcm::widgets::constants::kCircleDiameter;
  constexpr auto circleMargin = pcm::widgets::constants::kCircleMargin;

  // Draw the indicator circle if this button is selected
  if (isChecked()) {
    painter.setBrush(pcm::widgets::constants::kCircleColor);
    painter.setPen(Qt::NoPen);
    const QRect circleRect(circleMargin, (height() - circleDiameter) / 2,
                           circleDiameter, circleDiameter);
    painter.drawEllipse(circleRect);
  }

  // Text
  {
    painter.setPen(isChecked() ? palette().color(QPalette::Highlight)
                               : palette().color(QPalette::WindowText));

    constexpr auto textX = circleDiameter + 2 * circleMargin;
    const QRect textRect(textX, 0, width() - textX, height());
    painter.drawText(textRect, Qt::AlignCenter, mText);
  }
}

QSize TabButton::sizeHint() const {
  const QFontMetrics fm(font());
  constexpr auto circleDiameter = pcm::widgets::constants::kCircleDiameter;
  constexpr auto circleMargin = pcm::widgets::constants::kCircleMargin;
  const int w = fm.horizontalAdvance(mText) + circleDiameter + 3 * circleMargin;
  const int h = fm.height() + 4; // some padding at top and bottom
  return {w, h};
}