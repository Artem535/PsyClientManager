#include "eventview.h"
#include "constants.hpp"
#include "eventitem.h"
#include <qdatetime.h>
#include <qgraphicsscene.h>
#include <qline.h>
#include <qlogging.h>
#include <qloggingcategory.h>
#include <qminmax.h>
#include <qnamespace.h>

Q_LOGGING_CATEGORY(logEventView, "pcm.EventView")


EventView::EventView(QWidget *parent) : QGraphicsView(parent) {
  mScene = new QGraphicsScene(this);
  setScene(mScene);

  setRenderHint(QPainter::Antialiasing);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  updateSceneSize();
}

QGraphicsScene *EventView::getScene() { return mScene; }

void EventView::drawBackground(QPainter *painter, const QRectF &rect) {
  QGraphicsView::drawBackground(painter, rect);

  painter->save();
  painter->setPen(QPen(Qt::gray, 1.0, Qt::DashLine));
  // Draw vertical line that is centered in the viewport
  // TODO: It needed the rect.x()?
  const qreal xMid = rect.x() + rect.width() / 2;
  painter->drawLine(QLineF{xMid, rect.top(), xMid, rect.bottom()});

  painter->restore();
}

void EventView::updateSceneSize() {
  const QSize viewportSize = viewport()->size();

  const auto sceneHeight =
      qMax(viewportSize.height(), pcm::widgets::constants::kMinTimeAxisHeight);

  mPixelPerMin =
      static_cast<qreal>(sceneHeight) / pcm::widgets::constants::kMinInDay;
  mPixelPerMin = qBound(0.5, mPixelPerMin, 5.0);

  scene()->setSceneRect(0, 0, viewportSize.width(), sceneHeight);
}

void EventView::onEventSelected() {
  EventItem *item = qobject_cast<EventItem *>(sender());
  
  qCDebug(logEventView) << "EventView::onEventSelected| " << item;
  
  if (item != nullptr) {
    emit eventSelected(item);
  }
}

void EventView::updateItemsSize() {
  const QSize viewportSize = viewport()->size();

  for (QGraphicsItem *item : scene()->items()) {
    if (const auto eventItem = dynamic_cast<EventItem *>(item);
        eventItem != nullptr) {
      eventItem->updateSize(
          {viewportSize.width() / 2, eventItem->getSize().height()});
      eventItem->update();
    }
  }
}

void EventView::updateItemsCords() {
  for (QGraphicsItem *item : scene()->items()) {
    if (const auto eventItem = dynamic_cast<EventItem *>(item);
        eventItem != nullptr) {

      const auto datetimePoint = eventItem->getStartTime();
      const auto timePoint = datetimePoint.time();
      const auto countMin = timePoint.minute() + timePoint.hour() * 60;
      const auto yPos = countMin * mPixelPerMin;

      QPointF newPos{0, yPos};

      eventItem->setPos(newPos);
      eventItem->update();
    }
  }
}

void EventView::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  updateSceneSize();
  updateItemsSize();
  updateItemsCords();
}