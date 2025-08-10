#pragma once

#include "constants.hpp"
#include "eventitem.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLoggingCategory>
#include <QPainter>
#include <QVector>
#include <QWidget>
#include <memory>
#include <qgraphicsscene.h>
#include <qgraphicsview.h>

class QEventView final : public QGraphicsView {
  Q_OBJECT

public:
  explicit QEventView(QWidget *parent = nullptr);
  [[nodiscard]] QGraphicsScene *getScene() const;

signals:
  void eventSelected(QEventItem *item);

public slots:
  void updateScene();

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QGraphicsScene *mScene;
  int64_t mSelectedDay = -1;
  qreal mPixelPerMin = pcm::widgets::constants::kPixelPerMin;

  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void updateSceneSize();
  void updateItemsSize() const;
  void updateItemsCords() const;

private slots:
  void onEventSelected();
};