#include "eventitem.h"

// Qt
#include <QGraphicsScene>
#include <QLoggingCategory>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

// Standard library
#include <cmath>

Q_LOGGING_CATEGORY(logPcmEventItem, "pcm.EventItem")

EventItem::EventItem(long long id, const QString &title,
                     const QDateTime &startTime, const QDateTime &endTime,
                     bool isWorkItem)
    : mIsWorkItem(isWorkItem), mTitle(title),
      mStartTime(startTime), mEndTime(endTime), mId(id), mSize(100, 100) {

  qCInfo(logPcmEventItem) << "Created event:" << title
                          << "(" << startTime.toString() << "-"
                          << endTime.toString() << ")";

  
  
  setFlag(QGraphicsItem::ItemIsMovable);
  setFlag(QGraphicsItem::ItemIsSelectable);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
}

QRectF EventItem::boundingRect() const {
  const qreal penWidth = 1.0;
  const auto xOffset = mIsWorkItem ? 0 : mSize.width();
  return QRectF(xOffset + penWidth, penWidth, mSize.width() + penWidth,
                mSize.height() + penWidth);
}

void EventItem::updateSize(const QSize &newSize) {
  if (mSize == newSize)
    return;
  mSize = newSize;
  emit prepareGeometryChange();
  update();
}

QSize EventItem::getSize() const { return mSize; }

QDateTime EventItem::getStartTime() const { return mStartTime; }
QDateTime EventItem::getEndTime() const { return mEndTime; }
QString EventItem::getTitle() const { return mTitle; }
long long EventItem::getId() const { return mId; }
bool EventItem::isWorkItem() const {return mIsWorkItem;};

void EventItem::setTitle(const QString &title) {
  if (mTitle == title)
    return;
  mTitle = title;
  update();
}

void EventItem::setStartTime(const QDateTime &startTime) {
  if (mStartTime == startTime)
    return;
  mStartTime = startTime;
  update();
}

void EventItem::setEndTime(const QDateTime &endTime) {
  if (mEndTime == endTime)
    return;
  mEndTime = endTime;
  update();
}

void EventItem::setIsWorkItem(bool isWorkItem) {
  if (mIsWorkItem == isWorkItem)
    return;
  mIsWorkItem = isWorkItem;
  emit prepareGeometryChange();
  update();
}

void EventItem::setId(long long id) {
  mId = id;
  qCDebug(logPcmEventItem) << "EventItem ID changed to:" << mId;
}

QVariant EventItem::itemChange(GraphicsItemChange change,
                               const QVariant &value) {
  switch (change) {
  case ItemPositionChange: {
    auto newPos = value.toPointF();
    if (std::abs(newPos.x()) > mSize.width()) {
      qreal centerX = scene()->sceneRect().center().x();
      bool newWorkItem = newPos.x() < centerX;
      if (mIsWorkItem != newWorkItem) {
        mIsWorkItem = newWorkItem;
        emit prepareGeometryChange();
        update();
      }
    }
    return QPointF{0, newPos.y()};
  }

  default:
    break;
  }

  return QGraphicsObject::itemChange(change, value);
}

void EventItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  emit itemSelected();
  qCInfo(logPcmEventItem) << "EventItem::mousePressEvent| Item selected:" << mTitle;
  QGraphicsObject::mousePressEvent(event);
}

void EventItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                      QWidget *widget) {
  Q_UNUSED(option)
  Q_UNUSED(widget)

  painter->save();

  // Fill and border colors
  QColor fillColor = mIsWorkItem ? QColor(173, 216, 230)  // Light blue
                                : QColor(255, 182, 193); // Light pink
  QPen borderPen(mIsWorkItem ? Qt::darkBlue : Qt::darkRed, 1.5);

  painter->setBrush(fillColor);
  painter->setPen(borderPen);

  // Position of the rectangle
  const int x = mIsWorkItem ? 0 : mSize.width();
  painter->drawRoundedRect(x, 0, mSize.width(), mSize.height(), 5, 5);

  // Draw title text
  painter->setPen(Qt::white);
  const qreal margin_y = 0.1 * mSize.height();
  const qreal margin_x = 0.05 * mSize.width();
  const QRectF textRect(x + margin_x, margin_y,
                        mSize.width() * 0.9, mSize.height() * 0.8);
  painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, mTitle);

  painter->restore();
}