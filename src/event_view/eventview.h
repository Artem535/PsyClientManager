#pragma once

#include "eventitem.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QVector>
#include <QWidget>
#include <qgraphicsscene.h>
#include <qgraphicsview.h>
#include <QLoggingCategory>
#include <memory>

class EventView : public QGraphicsView {
  Q_OBJECT

public:
  explicit EventView(QWidget *parent = nullptr);
  QGraphicsScene *getScene();

signals:
  void eventSelected(std::shared_ptr<EventItem> item);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QGraphicsScene *mScene;
  int64_t mSelectedDay;
  qreal mPixelPerMin = 1;

  QVector<EventItem *> mEvents;

  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void updateSceneSize();
  void updateItemsSize();
  void updateItemsCords();

private slots:
  void onEventSelected();
};