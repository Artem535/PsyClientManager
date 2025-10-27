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
  setFont(QFont("Inter", pcm::widgets::constants::kFontSize));
}

void TabButton::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Transparent background
  painter.setBrush(Qt::NoBrush);
  painter.setPen(Qt::NoPen);
  painter.drawRect(rect());

  if (isChecked()) {
    const QPen pen{palette().color(QPalette::Highlight), 2};
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    // Draw rounded rect
    const QRect rect{2, 2, width() - 4, height() - 4};
    painter.drawRoundedRect(rect,  10, 10);
  }

  // Text
  {
    painter.setPen(isChecked() ? palette().color(QPalette::Highlight)
                               : palette().color(QPalette::WindowText));

    constexpr auto textX = 10;
    const QRect textRect(textX, 0, width() - textX, height());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, mText);
  }
}

QSize TabButton::sizeHint() const {
  const QFontMetrics fm(font());
  const int w = fm.horizontalAdvance(mText);
  const int h = fm.height() + 4; // some padding at top and bottom
  return {w, h};
}
