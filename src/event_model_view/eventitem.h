#pragma once

#include <QDateTime>
#include <QGraphicsRectItem>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTextItem>
#include <memory>
#include <qdatetime.h>
#include <qgraphicsitem.h>
#include <qobject.h>
#include <qsize.h>
#include <qvariant.h>

/**
 * @brief The EventItem class represents a graphical item for displaying an
 * event in a timeline or calendar view.
 *
 * This class inherits from QGraphicsItem and provides functionality to render
 * and manage event items visually.
 */
class EventItem : public QGraphicsObject {
  Q_OBJECT

public:
  /**
   * @brief Constructs a new EventItem with the specified title, start and end
   * times, size, and work item flag.
   *
   * @param title The title of the event.
   * @param startTime The start time of the event.
   * @param endTime The end time of the event.
   * @param size The size of the event item.
   * @param isWorkItem A flag indicating whether this is a work-related event
   * item.
   */
  EventItem(const QString &title, const QDateTime &startTime,
            const QDateTime &endTime, QSize size = {20, 20},
            bool isWorkItem = false);

  /**
   * @brief Returns the bounding rectangle of the item.
   *
   * @return The bounding rectangle.
   */
  QRectF boundingRect() const override;

  /**
   * @brief Paints the event item using the provided painter.
   *
   * @param painter The QPainter used for rendering.
   * @param option Style options for painting.
   * @param widget Parent widget (optional).
   */
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;

  /**
   * @brief Updates the size of the event item.
   *
   * @param size The new size.
   */
  void updateSize(const QSize &size);

  /**
   * @brief Returns the current size of the event item.
   *
   * @return The size.
   */
  QSize getSize() const;

  /**
   * @brief Returns the start time of the event.
   *
   * @return The start time.
   */
  QDateTime getStartTime() const;

  /**
   * @brief Returns the end time of the event.
   *
   * @return The end time.
   */
  QDateTime getEndTime() const;

  QString getTitle() const;

  /**
   * @brief Handles changes to the item's properties.
   *
   * @param change The type of change.
   * @param value The new value of the changed property.
   * @return The new value after handling the change.
   */
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant &value) override;

signals:
  void itemSelected();

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
  /**< Indicates whether the item is a work-related event. */
  bool mIsWorkItem = false;
  /**< The size of the event item. */
  QSize mSize;
  /**< The title of the event. */
  QString mTitle;
  /**< The start time of the event. */
  QDateTime mStartTime;
  /**< The end time of the event. */
  QDateTime mEndTime;
};
;