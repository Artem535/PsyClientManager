#pragma once

#include <QDateTime>
#include <QGraphicsRectItem>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTextItem>
#include <memory>
#include <qdatetime.h>
#include <qsize.h>
#include <qvariant.h>

class EventItem : public QGraphicsItem {
public:
  EventItem(const QString &title, const QDateTime &startTime,
            const QDateTime &endTime, QSize size = {20, 20},
            bool isWorkItem = false);

  QRectF boundingRect() const override;

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;

  void updateSize(const QSize &size);

  QSize getSize() const;
  QDateTime getStartTime() const;
  QDateTime getEndTime() const;

  QVariant itemChange(GraphicsItemChange change,
                      const QVariant &value) override;

private:
  bool mIsWorkItem = false;
  QSize mSize;
  QString mTitle;
  QDateTime mStartTime;
  QDateTime mEndTime;
};