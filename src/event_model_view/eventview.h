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


signals:
  void eventSelected(EventItem *item);

protected:
  void resizeEvent(QResizeEvent *event) override;


private:
  QGraphicsScene *mScene;
  int64_t mSelectedDay;
  qreal mPixelPerMin = 1;

  QVector<EventItem*> mEvents;

  // TODO: We need this?
  EventItem *mSelectedEvent = nullptr;

  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void updateSceneSize();
  void addDemoItems();
  void updateItemsSize();
  void updateItemsCords();

private slots:
  void onEventSelected();
};