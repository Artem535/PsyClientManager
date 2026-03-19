//
// Created by a.durynin on 29.08.2025.
//

#pragma once

#include <QDateTime>
#include <QHash>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>

#include <cmath>

#include "constants.hpp"
#include "qclient_model.h"

/**
 * @class QClientDelegate
 * @brief Custom delegate for rendering client cards in a 5-column layout.
 *
 * Columns:
 * 1) Name + Surname + Age
 * 2) Contacts (email, phone)
 * 3) Last session date
 * 4) Status chip (Active/Inactive)
 * 5) Action buttons (two icons)
 */
class QClientDelegate final : public QStyledItemDelegate {
  Q_OBJECT

signals:
  void displayButtonClicked(const QModelIndex &index);
  void removeButtonClicked(const QModelIndex &index);

public:
  /**
   * @brief Construct a new QClientDelegate object.
   * @param parent Optional parent object.
   */
  explicit QClientDelegate(QObject *parent = nullptr);

  /**
   * @brief Returns the size hint for each client card.
   * @param option Style options.
   * @param index Model index of the item.
   * @return QSize The preferred size of the card.
   */
  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                               const QModelIndex &index) const override;

  /**
   * @brief Paints a single client card item using a 5-column layout.
   * @param painter The painter used for drawing.
   * @param option Style options for the item.
   * @param index Model index of the item.
   */
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

  /**
   * @brief Draws the first column (Name, Surname, Age).
   * @param painter The painter used for drawing.
   * @param option Style options for the item.
   * @param client The client data to display.
   */
  static void drawFirstColumn(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const DuckClient &client);

  /**
   * @brief Draws the second column (Email, Phone).
   * @param painter The painter used for drawing.
   * @param option Style options for the item.
   * @param client The client data to display.
   */
  static void drawContacts(QPainter *painter,
                           const QStyleOptionViewItem &option,
                           const DuckClient &client);

  /**
   * @brief Draws the third column (Last session date).
   * @param painter The painter used for drawing.
   * @param option Style options for the item.
   * @param client The client data to display.
   */
  static void drawLastSession(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const DuckClient &client);

  /**
   * @brief Draws the fourth column (Status chip).
   * @param painter The painter used for drawing.
   * @param option Style options for the item.
   * @param client The client data to display.
   */
  static void drawStatusChip(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const DuckClient &client);

  /**
   * @brief Draws the fifth column (two action buttons) and update them coords
   * in buttonRects.
   * @param painter The painter used for drawing.
   * @param option Style options for the item.
   * @param client The client data (not used here, reserved for future).
   */
  static void drawActions(QPainter *painter, const QStyleOptionViewItem &option,
                          const DuckClient &client);

  static std::pair<QRect, QRect>
  calculateButtonRects(const QStyleOptionViewItem &option);

  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

private:
  /**
   * @brief Helper to calculate age based on a birthdate.
   * @param birthDate Date of birth.
   * @return int Calculated age.
   */
  static int countAge(const QDate &birthDate);

  mutable QHash<QPersistentModelIndex, std::pair<QRect, QRect>> mButtonRects;
};
