#include "eventitem.h"
#include <qdatetime.h>
#include <qgraphicsitem.h>
#include <qgraphicsscene.h>
#include <qlogging.h>
#include <qloggingcategory.h>
#include <qnamespace.h>
#include <qpainter.h>
#include <qpoint.h>
#include <qvariant.h>

Q_LOGGING_CATEGORY(logPcmEventItem, "pcm.EventItem")

EventItem::EventItem(long long id, const QString &title,
                     const QDateTime &startTime, const QDateTime &endTime,
                     bool isWorkItem, QSize size)
    : mIsWorkItem(isWorkItem), mSize(size), mTitle(title),
      mStartTime(startTime), mEndTime(endTime), mId(id) {

  qCInfo(logPcmEventItem) << "Created event: " << title << " (" << startTime.toString() << " - "
          << endTime.toString() << ")";

          setFlag(QGraphicsItem::ItemIsMovable);
  setFlag(QGraphicsItem::ItemIsSelectable);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
}

QRectF EventItem::boundingRect() const {
  const qreal penWidth{1};
  const auto xOffset = mIsWorkItem ? 0 : mSize.width();
  return QRectF(xOffset + penWidth, penWidth, mSize.width() + penWidth,
                mSize.height() + penWidth);
}

void EventItem::updateSize(const QSize &newSize) {
  mSize = newSize;
  emit prepareGeometryChange();
}

QSize EventItem::getSize() const { return mSize; }

QVariant EventItem::itemChange(GraphicsItemChange change,
                               const QVariant &value) {
  switch (change) {
  case ItemPositionChange: {
    auto newPos = value.toPointF();

    if (const auto dx = newPos.x(); abs(dx) > mSize.width()) {
      qreal centerX = scene()->sceneRect().center().x();
      mIsWorkItem = newPos.x() < centerX;
    }

    return QPointF{0, newPos.y()};
  }

  default:
    break;
  }

  return QGraphicsItem::itemChange(change, value);
}

QDateTime EventItem::getStartTime() const { return mStartTime; }
QDateTime EventItem::getEndTime() const { return mEndTime; }
QString EventItem::getTitle() const { return mTitle; }

void EventItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  emit itemSelected();
  qCInfo(logPcmEventItem) << "EventItem::mousePressEvent| Item selected: "
                          << mTitle;
  QGraphicsItem::mousePressEvent(event);
}

void EventItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                      QWidget *widget) {

  painter->save();

  // Style setup
  QColor fillColor =
      mIsWorkItem ? QColor(173, 216, 230) : QColor(255, 182, 193);
  QPen borderPen(mIsWorkItem ? Qt::darkBlue : Qt::darkRed, 1.5);

  // Draw event rectangle
  painter->setBrush(fillColor);
  painter->setPen(borderPen);

  const int x = mIsWorkItem ? 0 : mSize.width();
  painter->drawRoundedRect(x, 0, mSize.width(), mSize.height(), 5, 5);

  // Draw event title
  painter->setPen(QPen(Qt::white));
  const qreal margin_y{0.1 * mSize.height()};
  const qreal margin_x{0.05 * mSize.width()};
  const auto y = static_cast<int>(round(margin_y));
  const auto max_width = static_cast<int>(round(mSize.width() * 0.9));
  const auto max_height = static_cast<int>(round(mSize.height() * 0.8));
  const QRectF textRect(x + margin_x, y, max_width, max_height);
  painter->drawText(textRect, mTitle);

  painter->restore();
}