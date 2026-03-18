#include "event_item.h"
#include "../widgets/app_settings.h"
#include <QIcon>
#include <QLocale>
#include <QMenu>
#include <QTimeZone>

Q_LOGGING_CATEGORY(logPcmEventItem, "pcm.EventItem")

namespace {
QString formatEventCost(const std::optional<double>& cost) {
  if (!cost.has_value()) {
    return {};
  }

  return QLocale(QLocale::Russian).toString(*cost, 'f', 0) + QStringLiteral(" ₽");
}
} // namespace

QEventItem::QEventItem(const unsigned long id, const QString &title,
                       const QDateTime &startTime, const QDateTime &endTime,
                       const bool isWorkItem)
    : mIsWorkItem(isWorkItem), mSize(100, 100), mTitle(title),
      mStartTime(startTime), mEndTime(endTime), mId(id) {
  qCInfo(logPcmEventItem) << "QEventItem::QEventItem"
                          << "Created event:" << title << "("
                          << startTime.toString() << "-" << endTime.toString()
                          << ")";

  setFlag(QGraphicsItem::ItemIsMovable);
  setFlag(QGraphicsItem::ItemIsSelectable);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);

  updateDuration();
}

void QEventItem::updateFromEvent(const DuckEvent &event) {
  mIsWorkItem = event.is_work_event;
  mSize = QSize(100, 100);

  mTitle = QString::fromStdString(event.name.value_or(""));
  mClientName = QString::fromStdString(event.client_name.value_or(""));
  mCost = event.cost;
  const auto startUtc =
      QDateTime::fromMSecsSinceEpoch(event.start_date.value_or(0), QTimeZone::UTC);
  const auto endUtc =
      QDateTime::fromMSecsSinceEpoch(event.end_date.value_or(0), QTimeZone::UTC);
  mStartTime = startUtc.toLocalTime();
  mEndTime = endUtc.toLocalTime();
  mId = event.id;

  qCInfo(logPcmEventItem) << "QEventItem::QEventItem"
                          << "Created event:" << mTitle << "("
                          << mStartTime.toString() << "-" << mEndTime.toString()
                          << ")";

  setFlag(QGraphicsItem::ItemIsMovable);
  setFlag(QGraphicsItem::ItemIsSelectable);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);

  updateDuration();
  update();
}

QEventItem::QEventItem(const DuckEvent &event) {
  mIsWorkItem = event.is_work_event;
  mSize = QSize(100, 100);
  mTitle = QString::fromStdString(event.name.value_or("Undefined"));
  mClientName = QString::fromStdString(event.client_name.value_or(""));
  mCost = event.cost;
  const auto startUtc =
      QDateTime::fromMSecsSinceEpoch(event.start_date.value_or(0), QTimeZone::UTC);
  const auto endUtc =
      QDateTime::fromMSecsSinceEpoch(event.end_date.value_or(0), QTimeZone::UTC);
  mStartTime = startUtc.toLocalTime();
  mEndTime = endUtc.toLocalTime();
  mId = event.id;

  qCInfo(logPcmEventItem) << "QEventItem::QEventItem"
                          << "Created event:" << mTitle << "("
                          << mStartTime.toString() << "-" << mEndTime.toString()
                          << ")";

  setFlag(QGraphicsItem::ItemIsMovable);
  setFlag(QGraphicsItem::ItemIsSelectable);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);

  updateDuration();
}

DuckEvent QEventItem::toEvent() const {
  DuckEvent event;
  event.id = mId;
  event.name = mTitle.toStdString();
  event.is_work_event = mIsWorkItem;
  event.start_date = mStartTime.toUTC().toMSecsSinceEpoch();
  event.end_date = mEndTime.toUTC().toMSecsSinceEpoch();
  event.duration = mDuration;
  event.cost = mCost;
  return event;
}

QRectF QEventItem::boundingRect() const {
  constexpr qreal penWidth = 1.0;
  constexpr int labelWidth = pcm::widgets::constants::kWidthLabel;

  auto xOffset = mIsWorkItem ? 0 : mSize.width();
  xOffset += labelWidth;

  return {xOffset + penWidth, penWidth, mSize.width() + penWidth,
          mSize.height() + penWidth};
}

void QEventItem::updateSize(const QSize &newSize) {
  if (mSize == newSize)
    return;
  mSize = newSize;

  constexpr int offset = pcm::widgets::constants::kWidthLabel / 2;
  mSize.setWidth(mSize.width() - offset);

  emit prepareGeometryChange();
  update();
}

QSize QEventItem::getSize() const { return mSize; }
QDateTime QEventItem::getStartTime() const { return mStartTime; }
QDateTime QEventItem::getEndTime() const { return mEndTime; }
QString QEventItem::getTitle() const { return mTitle; }
QString QEventItem::getClientName() const { return mClientName; }
std::optional<double> QEventItem::cost() const { return mCost; }
unsigned long QEventItem::getId() const { return mId; }
bool QEventItem::isWorkItem() const { return mIsWorkItem; };

void QEventItem::setTitle(const QString &title) {
  if (mTitle == title)
    return;
  mTitle = title;
  update();
}

void QEventItem::setClientName(const QString &clientName) {
  if (mClientName == clientName)
    return;
  mClientName = clientName;
  update();
}

void QEventItem::setCost(std::optional<double> cost) {
  if (mCost == cost)
    return;
  mCost = std::move(cost);
  update();
}

void QEventItem::setStartTime(const QDateTime &startTime) {
  if (mStartTime == startTime)
    return;

  if (startTime > mEndTime) {
    qCWarning(logPcmEventItem) << "QEventItem::setStartTime"
                               << "Invalid start time:" << startTime;
    return;
  }

  mStartTime = startTime;
  updateDuration();
  update();
}

void QEventItem::setEndTime(const QDateTime &endTime) {
  if (mEndTime == endTime)
    return;

  if (endTime < mStartTime) {
    qCWarning(logPcmEventItem) << "QEventItem::setEndTime"
                               << "Invalid end time:" << endTime;
    return;
  }

  mEndTime = endTime;
  updateDuration();
  update();
}

void QEventItem::setTimeRange(const QDateTime &startTime,
                              const QDateTime &endTime) {
  if (startTime > endTime) {
    qCWarning(logPcmEventItem) << "QEventItem::setTimeRange"
                               << "Invalid time range:" << startTime << endTime;
    return;
  }

  if (mStartTime == startTime && mEndTime == endTime) {
    return;
  }

  mStartTime = startTime;
  mEndTime = endTime;
  updateDuration();
  update();
}

void QEventItem::setIsWorkItem(const bool isWorkItem) {
  if (mIsWorkItem == isWorkItem)
    return;
  mIsWorkItem = isWorkItem;
  emit prepareGeometryChange();
  update();
}

void QEventItem::setId(const long long id) {
  mId = id;
  qCDebug(logPcmEventItem) << "EventItem ID changed to:" << mId;
}

unsigned int QEventItem::getDuration() const { return mDuration; }

QVariant QEventItem::itemChange(const GraphicsItemChange change,
                                const QVariant &value) {
  switch (change) {
  case ItemPositionChange: {
    const auto newPos = value.toPointF();
    if (std::abs(newPos.x()) > mSize.width()) {
      const qreal centerX = scene()->sceneRect().center().x();
      if (const bool newWorkItem = newPos.x() < centerX;
          mIsWorkItem != newWorkItem) {
        mIsWorkItem = newWorkItem;
        emit prepareGeometryChange();
        update();
      }
    }
    return QPointF{0, newPos.y()};
  }

  default:
    break;
  }

  return QGraphicsObject::itemChange(change, value);
}

void QEventItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  if (event && event->button() == Qt::LeftButton) {
    emit itemSelected();
    qCInfo(logPcmEventItem) << "EventItem::mousePressEvent| Item selected:"
                            << mTitle;
  }
  QGraphicsObject::mousePressEvent(event);
}

void QEventItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
  if (!event) {
    return;
  }

  QMenu menu;
  auto *editAction = menu.addAction(tr(": EVENT_CONTEXT_EDIT"));
  auto *deleteAction = menu.addAction(tr(": EVENT_CONTEXT_DELETE"));
  connect(editAction, &QAction::triggered, this,
          [this]() { emit editRequested(); });
  connect(deleteAction, &QAction::triggered, this,
          [this]() { emit deleteRequested(); });
  menu.exec(event->screenPos());

  event->accept();
}

void QEventItem::updateDuration() {
  auto startTimeMin = mStartTime.time().minute();
  startTimeMin += mStartTime.time().hour() * 60;

  auto endTimeMin = mEndTime.time().minute();
  endTimeMin += mEndTime.time().hour() * 60;

  mDuration = endTimeMin - startTimeMin;
}

void QEventItem::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget) {
  Q_UNUSED(option)
  Q_UNUSED(widget)

  painter->save();

  // Fill and border colors
  const QColor fillColor = mIsWorkItem ? pcm::app_settings::workEventColor()
                                       : pcm::app_settings::personalEventColor();
  const QPen borderPen(fillColor.darker(165), 1.5);

  painter->setBrush(fillColor);
  painter->setPen(borderPen);

  // Position of the rectangle
  constexpr int xOffset = pcm::widgets::constants::kWidthLabel;
  const int x = mIsWorkItem ? xOffset : xOffset + mSize.width();

  painter->drawRoundedRect(x, 0, mSize.width(), mSize.height(), 5, 5);

  painter->setPen(Qt::white);
  const qreal margin_y = 0.12 * mSize.height();
  const qreal margin_x = 0.06 * mSize.width();

  if (mIsWorkItem) {
    constexpr int iconSize = 12;
    constexpr int iconTextSpacing = 6;
    const auto briefcasePixmap = QIcon(":/icons/briefcase-solid-full.svg")
                                     .pixmap(iconSize, iconSize);
    const auto userPixmap =
        QIcon(":/icons/user-solid-full.svg").pixmap(iconSize, iconSize);
    const auto coinsPixmap =
        QIcon(":/icons/coins-solid-full.svg").pixmap(iconSize, iconSize);
    const auto costText = formatEventCost(mCost);
    const qreal left = x + margin_x;
    const qreal right = x + mSize.width() - margin_x;
    const qreal top = margin_y;
    const qreal bottom = mSize.height() - margin_y;
    const qreal rowHeight = 16.0;
    const qreal rowGap = 4.0;
    const qreal itemGap = 10.0;

    qreal cursorX = left;
    qreal cursorY = top;

    auto drawFlowEntry = [&](const QPixmap& icon, const QString& text,
                             const QFont& font) -> bool {
      if (text.isEmpty()) {
        return true;
      }

      const QFontMetricsF metrics(font);
      const qreal naturalWidth =
          icon.width() + iconTextSpacing + metrics.horizontalAdvance(text);
      const qreal maxWidth = right - left;

      if (cursorX > left && cursorX + naturalWidth > right) {
        cursorX = left;
        cursorY += rowHeight + rowGap;
      }

      if (cursorY + rowHeight > bottom) {
        return false;
      }

      const qreal remainingWidth = right - cursorX;
      const qreal textWidth =
          std::max<qreal>(0.0, remainingWidth - icon.width() - iconTextSpacing);
      if (textWidth <= 6.0) {
        return false;
      }

      painter->setFont(font);
      painter->drawPixmap(QPointF(cursorX, cursorY + (rowHeight - icon.height()) / 2.0),
                          icon);

      const auto elidedText = metrics.elidedText(text, Qt::ElideRight, textWidth);
      const QRectF textRect(cursorX + icon.width() + iconTextSpacing, cursorY,
                            textWidth, rowHeight);
      painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);

      cursorX += std::min(maxWidth, naturalWidth) + itemGap;
      return true;
    };

    QFont titleFont = painter->font();
    titleFont.setBold(true);
    QFont secondaryFont = painter->font();
    secondaryFont.setPointSizeF(secondaryFont.pointSizeF() - 0.5);

    if (!drawFlowEntry(briefcasePixmap, mTitle, titleFont)) {
      painter->restore();
      return;
    }
    drawFlowEntry(userPixmap, mClientName, secondaryFont);
    drawFlowEntry(coinsPixmap, costText, secondaryFont);
  } else {
    const QRectF textRect(x + margin_x, margin_y, mSize.width() * 0.9,
                          mSize.height() * 0.8);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, mTitle);
  }

  painter->restore();
}
