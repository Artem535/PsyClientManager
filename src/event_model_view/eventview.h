#pragma once

#include "eventitem.h"
#include "timelinewidgetconstants.hpp"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QWidget>
#include <QVector>
#include <memory>

class EventView : public QGraphicsView {
  Q_OBJECT

public:
  explicit EventView(QWidget *parent = nullptr);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QGraphicsScene *mScene;
  qreal mPixelPerMin = 1;

  QVector<EventItem*> mEvents;

  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void updateSceneSize();
  void addDemoItems();
  void updateItemsSize();
  void updateItemsCords();
};