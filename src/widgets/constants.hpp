#pragma once

#include <QColor>

namespace pcm::widgets::constants {
    constexpr auto kFontSize = 15;
    constexpr auto kPageContentMargin = 9;
    constexpr auto kPanelPadding = 10;
    constexpr auto kCircleDiameter = 10;
    constexpr auto kCircleMargin = kCircleDiameter / 2;
    constexpr auto kCircleColor = QColor(0, 120, 215);
    inline const QColor kSurfaceBorderColor = QColor(255, 255, 255, 31);
    inline const QColor kSurfaceBackgroundColor = QColor(255, 255, 255, 5);
    inline const QColor kCalendarCardBorderColor = QColor(255, 255, 255, 0);
    inline const QColor kCalendarCardBackgroundColor = QColor(255, 255, 255, 8);
    inline const QColor kCalendarCurrentDayUnderlineColor = QColor(0x9f, 0xc0, 0xff);
    inline const QColor kCalendarCurrentDayForegroundColor = QColor(0xd9, 0xe6, 0xff);
} // namespace pcm::widgets::constants
