#pragma once

#include <QDateTime>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QLoggingCategory>
#include <QPainter>
#include <QSize>
#include <QString>
#include <QStyleOptionGraphicsItem>
#include <QVariant>

#include <cmath>

#include "constants.hpp"
#include "scheme.obx.hpp"
/**
 * @brief The EventItem class represents a graphical item for displaying an
 * event in a timeline or calendar view.
 *
 * This class inherits from QGraphicsObject and provides functionality to render
 * and manage event items visually.
 */
class QEventItem final : public QGraphicsObject {
  Q_OBJECT

public:
  /**
   * @brief Constructs a new EventItem with the specified title, start and end
   * times, size, and work item flag.
   *
   * @param id Unique identifier for the event.
   * @param title The title of the event.
   * @param startTime The start time of the event.
   * @param endTime The end time of the event.
   * @param isWorkItem True if the event is a work-related item.
   */
  QEventItem(long long id, const QString &title, const QDateTime &startTime,
             const QDateTime &endTime, bool isWorkItem = false);

  QEventItem(const ObxEvent &event);

  /**
   * @brief Returns the bounding rectangle of the item.
   * @return The bounding rectangle.
   */
  [[nodiscard]] QRectF boundingRect() const override;

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
   * @param newSize The new size.
   */
  void updateSize(const QSize &newSize);

  /**
   * @brief Returns the current size of the event item.
   * @return The size.
   */
  [[nodiscard]] QSize getSize() const;

  /**
   * @brief Returns the start time of the event.
   * @return The start time.
   */
  [[nodiscard]] QDateTime getStartTime() const;

  /**
   * @brief Returns the end time of the event.
   * @return The end time.
   */
  [[nodiscard]] QDateTime getEndTime() const;

  /**
   * @brief Returns the title of the event.
   * @return The title.
   */
  [[nodiscard]] QString getTitle() const;

  /**
   * @brief Returns the unique identifier of the event.
   * @return The event ID.
   */
  [[nodiscard]] unsigned long getId() const;

  [[nodiscard]] bool isWorkItem() const;

  /**
   * @brief Sets the title of the event and triggers a repaint if changed.
   * @param title The new title.
   */
  void setTitle(const QString &title);

  /**
   * @brief Sets the start time of the event.
   * @param startTime The new start time.
   */
  void setStartTime(const QDateTime &startTime);

  /**
   * @brief Sets the end time of the event.
   * @param endTime The new end time.
   */
  void setEndTime(const QDateTime &endTime);

  /**
   * @brief Sets whether the event is a work-related item.
   *
   * Changing this may affect the visual position (left/right side) of the item.
   * @param isWorkItem True if it's a work item; false otherwise.
   */
  void setIsWorkItem(bool isWorkItem);

  /**
   * @brief Sets the unique identifier of the event.
   * @param id The new ID.
   */
  void setId(long long id);

  [[nodiscard]] unsigned int getDuration() const;
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
  /**
   * @brief Emitted when the item is clicked.
   */
  void itemSelected();

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
  bool mIsWorkItem = false;   ///< True if the event is a work-related item.
  QSize mSize;                ///< The size of the event item.
  QString mTitle;             ///< The title of the event.
  QDateTime mStartTime;       ///< The start time of the event.
  QDateTime mEndTime;         ///< The end time of the event.
  unsigned long mId;          ///< Unique identifier of the event.
  unsigned int mDuration = 0; ///< Duration of the event in minutes.

  void updateDuration();
};