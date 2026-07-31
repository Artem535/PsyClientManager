#pragma once

#include "constants.hpp"

#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QObject>
#include <QPainter>
#include <QPen>

namespace pcm::widgets {

// Paints a card-like surface background without QSS. setStyleSheet() wraps
// style() in a QStyleSheetStyle proxy for every descendant, which breaks
// oclero::qlementine::SegmentedControl's qobject_cast<QlementineStyle*>(style())
// and makes it fall back to flat QPalette colors. Use this instead of a
// stylesheet on any surface that hosts a SegmentedControl.
class SurfacePaintFilter : public QObject {
public:
  explicit SurfacePaintFilter(QObject *parent = nullptr,
                              const int cornerRadius = constants::kCardCornerRadius)
      : QObject(parent), mCornerRadius(cornerRadius) {}

protected:
  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::Paint) {
      auto *frame = qobject_cast<QFrame *>(obj);
      if (!frame) {
        return false;
      }
      QPainter p(frame);
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(QPen(QColor(255, 255, 255, 20), 1));
      p.setBrush(QColor(255, 255, 255, 13));
      const auto r = frame->rect();
      p.drawRoundedRect(r.adjusted(0, 0, -1, -1), mCornerRadius, mCornerRadius);
      return true; // fully handled
    }
    return QObject::eventFilter(obj, event);
  }

private:
  int mCornerRadius;
};

} // namespace pcm::widgets
