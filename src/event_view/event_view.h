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
  void eventEditRequested(QEventItem *item);
  void eventDeleteRequested(int64_t eventId);

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
  void onEventEditRequested();
  void onEventDeleteRequested();

private:
  QTimelineModel *mModel{};
  QGraphicsScene *mScene;
  int64_t mSelectedDay = -1;
  qreal mPixelPerMin = pcm::widgets::constants::kPixelPerMin;
  QMap<int64_t, QEventItem *> mSceneItems;

  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void updateSceneSize();
  void updateItemsSize() const;
  void updateItemsCords() const;
};
