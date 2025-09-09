#pragma once

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLoggingCategory>
#include <QPainter>
#include <QVector>
#include <QWidget>

#include <memory>

#include "constants.hpp"
#include "event_item.h"
#include "qtimeline_model.h"

class QEventView final : public QGraphicsView {
  Q_OBJECT

public:
  explicit QEventView(QWidget *parent = nullptr);
  void setModel(QTimelineModel *model);

signals:
  void eventSelected(QEventItem *item);

public slots:
  void updateScene();
  void onRowsInserted(const QModelIndex &parent, int first, int last);
  void onRowsRemoved(const QModelIndex &parent, int first, int last);
  void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                     const QList<int> &roles);
  void onModelReset();

protected:
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onEventSelected();

private:
  QTimelineModel *mModel{};
  QGraphicsScene *mScene;
  int64_t mSelectedDay = -1;
  qreal mPixelPerMin = pcm::widgets::constants::kPixelPerMin;
  QMap<obx_id, QEventItem *> mSceneItems;

  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void updateSceneSize();
  void updateItemsSize() const;
  void updateItemsCords() const;
};
