#include "eventview.h"
#include "eventitem.h"
#include "timelinewidgetconstants.hpp"
#include <ctime>
#include <qdatetime.h>
#include <qline.h>
#include <qlogging.h>
#include <qminmax.h>
#include <qnamespace.h>

EventView::EventView(QWidget *parent) : QGraphicsView(parent) {
  mScene = new QGraphicsScene(this);
  setScene(mScene);

  setRenderHint(QPainter::Antialiasing);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  updateSceneSize();
  addDemoItems();
}

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

void EventView::addDemoItems() {
  const int itemWidth = viewport()->width() / 2 - 10; // Ширина с отступом
  const QDateTime now = QDateTime::currentDateTime();

  mEvents.push_back(new EventItem{"Meeting", now, now.addSecs(3600),
                                  QSize(itemWidth, 100), true});
  mEvents.push_back(new EventItem{"Lunch", now.addSecs(7200), now.addSecs(9000),
                                  QSize(itemWidth, 80), false});

  for (const auto event : mEvents) {
    scene()->addItem(event);
    connect(event, &EventItem::itemSelected, this, &EventView::onEventSelected);
  }
}

void EventView::onEventSelected() {
  EventItem *item = qobject_cast<EventItem *>(sender());
  qInfo() << "EventView::onEventSelected| " << item;
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