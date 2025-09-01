#include "event_view.h"


Q_LOGGING_CATEGORY(logEventView, "pcm.EventView")

QEventView::QEventView(QWidget *parent) : QGraphicsView(parent) {
  mScene = new QGraphicsScene(this);
  setScene(mScene);

  setRenderHint(QPainter::Antialiasing);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  updateSceneSize();
}

QGraphicsScene *QEventView::getScene() const { return mScene; }

void QEventView::drawBackground(QPainter *painter, const QRectF &rect) {
  QGraphicsView::drawBackground(painter, rect);

  painter->save();

  // DRAW HORIZONTAL AXIS
  {
    painter->setPen(QPen(Qt::gray, 1.0, Qt::DashLine));
    // Draw vertical line that is centered in the viewport
    // TODO: It needed the rect.x()?
    qreal xMid = rect.x() + rect.width() / 2;
    xMid += pcm::widgets::constants::kWidthLabel / 2.0;
    painter->drawLine(QLineF{xMid, rect.top(), xMid, rect.bottom()});
  }

  // Draw separator line
  {
    painter->setPen(QPen(Qt::lightGray, 1.0, Qt::SolidLine));
    constexpr int xCord = pcm::widgets::constants::kWidthLabel;
    painter->drawLine(QLineF(xCord, 0, xCord, rect.bottom()));
  }

  // DRAW TIME AXIS
  {
    constexpr int startHour = 0;
    constexpr int endHour = 24;

    QFont font = painter->font();
    font.setPointSize(8);
    painter->setFont(font);

    for (int hour = startHour; hour < endHour; ++hour) {
      constexpr int stepMinutes = 60;
      const int totalMinutes = hour * stepMinutes;
      const qreal yPos = totalMinutes * mPixelPerMin;

      // Draw horizontal line
      painter->drawLine(QLineF(rect.left(), yPos, rect.right(), yPos));

      // Label for time: HH:00
      auto timeLabel = QString("%1:00");
      // FileWidth - 2 char, filled with 0.
      timeLabel = timeLabel.arg(hour, 2, 10, QChar('0'));

      constexpr int widthLabel = pcm::widgets::constants::kWidthLabel;
      QRectF labelRect(rect.left(), yPos, widthLabel, 20);

      painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignVCenter,
                        timeLabel);
    }
  }

  painter->restore();
}

void QEventView::updateSceneSize() {
  const QSize viewportSize = viewport()->size();

  const auto sceneHeight =
      qMax(viewportSize.height(), pcm::widgets::constants::kMinTimeAxisHeight);

  mPixelPerMin =
      static_cast<qreal>(sceneHeight) / pcm::widgets::constants::kMinInDay;
  mPixelPerMin = qBound(0.5, mPixelPerMin, 5.0);

  scene()->setSceneRect(0, 0, viewportSize.width(), sceneHeight);
}

void QEventView::onEventSelected() {
  auto *item = qobject_cast<QEventItem *>(sender());
  qCInfo(logEventView) << "EventDataManager::onEventSelected| "
                       << item->getId();
  if (item != nullptr) {
    emit eventSelected(item);
  }
}

void QEventView::updateItemsSize() const {
  const QSize viewportSize = viewport()->size();

  for (QGraphicsItem *item : scene()->items()) {
    if (const auto eventItem = dynamic_cast<QEventItem *>(item);
        eventItem != nullptr) {

      // We need to update height of eventItem,
      // it must be equal the duration of the event
      const auto duration = eventItem->getDuration();
      const auto height = static_cast<int>(round(duration * mPixelPerMin));

      eventItem->updateSize({viewportSize.width() / 2, height});
      eventItem->update();
    }
  }
}

void QEventView::updateItemsCords() const {
  for (QGraphicsItem *item : scene()->items()) {
    if (const auto eventItem = dynamic_cast<QEventItem *>(item);
        eventItem != nullptr) {

      const auto datetimePoint = eventItem->getStartTime();

      if (datetimePoint.timeSpec() != Qt::LocalTime)
        qCWarning(logEventView) << "EventItem has non-local time!";

      const auto timePoint = datetimePoint.time();
      const auto countMin = timePoint.minute() + timePoint.hour() * 60;
      const auto yPos = countMin * mPixelPerMin;

      QPointF newPos{0, yPos};

      eventItem->setPos(newPos);
      eventItem->update();
    }
  }
}

void QEventView::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  updateScene();
}

void QEventView::updateScene() {
  updateSceneSize();
  updateItemsSize();
  updateItemsCords();
}