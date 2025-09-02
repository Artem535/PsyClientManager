//
// Created by a.durynin on 29.08.2025.
//

#include "qclient_delegate.h"

namespace {
// Convenience aliases
namespace cst = pcm::widgets::constants;

// Compute absolute widths based on fractions
inline int widthBy(const double fraction, const int total) {
  return static_cast<int>(std::round(fraction * total));
}
} // namespace

QClientDelegate::QClientDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QSize QClientDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex & /*index*/) const {
  const int width = option.rect.width();
  constexpr int height = cst::kCardHeight;
  return {width, height};
}

void QClientDelegate::paint(QPainter *painter,
                            const QStyleOptionViewItem &option,
                            const QModelIndex &index) const {
  painter->save();

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setRenderHint(QPainter::TextAntialiasing, true);
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Draw background
  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  if (opt.widget) {
    opt.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt,
                                       painter, opt.widget);
  }

  const QVariant clientVal = index.data(QClientModel::ClientRoles::Full_object);
  const auto client = clientVal.value<ObxClient>();

  drawFirstColumn(painter, option, client);
  drawContacts(painter, option, client);
  drawLastSession(painter, option, client);
  drawStatusChip(painter, option, client);
  drawActions(painter, option, client);

  painter->restore();
}

// ---------------- First column (Name, Surname, Age) ----------------
void QClientDelegate::drawFirstColumn(QPainter *painter,
                                      const QStyleOptionViewItem &option,
                                      const ObxClient &client) {
  const QRect cardRect = option.rect;
  const int cardWidth = cardRect.width();
  const int cardHeight = cardRect.height();
  constexpr int sideMargin = cst::kSideMargin;

  const int firstW = widthBy(cst::kFirstColumnWidth, cardWidth);

  // First column rectangle (with left padding)
  const QRect firstRect(cardRect.left() + sideMargin, cardRect.top(),
                        firstW - sideMargin, cardHeight);

  // Colors and fonts
  const QPalette palette = option.palette;
  painter->setPen(palette.color(QPalette::WindowText));

  QFont font = option.font;
  font.setBold(true);
  painter->setFont(font);

  // ---- Name + Surname ----
  const QString fullName = QString("%1 %2").arg(client.name, client.last_name);
  painter->drawText(firstRect, Qt::AlignLeft | Qt::AlignTop, fullName);

  // ---- Age ----
  font.setBold(false);
  painter->setFont(font);

  const QDate birthDate =
      QDateTime::fromSecsSinceEpoch(client.birthday_date).date();
  const int age = countAge(birthDate);
  const QString ageStr = QString("%1 лет").arg(age);

  const int halfH = cardHeight / 2;
  const QRect ageRect = firstRect.adjusted(0, halfH, 0, 0);
  painter->drawText(ageRect, Qt::AlignLeft | Qt::AlignTop, ageStr);
}

// ---------------- Second column (Email, Phone) ----------------
void QClientDelegate::drawContacts(QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const ObxClient &client) {
  const QRect cardRect = option.rect;
  const int cardWidth = cardRect.width();
  const int cardHeight = cardRect.height();
  constexpr int sideMargin = cst::kSideMargin;

  const int firstW = widthBy(cst::kFirstColumnWidth, cardWidth);
  const int secondW = widthBy(cst::kSecondColumnWidth, cardWidth);

  // Second column rectangle (starts after first column, with padding)
  const QRect contactsRect(cardRect.left() + firstW + sideMargin,
                           cardRect.top(), secondW - sideMargin * 2,
                           cardHeight);

  const QPalette palette = option.palette;
  painter->setPen(palette.color(QPalette::Text));

  QFont font = option.font;
  font.setPointSize(font.pointSize() - 1);
  painter->setFont(font);

  // ---- Email ----
  const QString email = client.email.empty()
                            ? QString("Нет")
                            : QString::fromStdString(client.email);
  painter->drawText(contactsRect, Qt::AlignLeft | Qt::AlignTop, email);

  // ---- Phone ----
  const QString phone = client.phone_number.empty()
                            ? QString("Нет")
                            : QString::fromStdString(client.phone_number);
  const int halfH = cardHeight / 2;
  const QRect phoneRect = contactsRect.adjusted(0, halfH, 0, 0);
  painter->drawText(phoneRect, Qt::AlignLeft | Qt::AlignTop, phone);
}

// ---------------- Third column (Last session date) ----------------
void QClientDelegate::drawLastSession(QPainter *painter,
                                      const QStyleOptionViewItem &option,
                                      const ObxClient &client) {
  const QRect cardRect = option.rect;
  const int cardWidth = cardRect.width();
  const int cardHeight = cardRect.height();
  constexpr int sideMargin = cst::kSideMargin;

  const int firstW = widthBy(cst::kFirstColumnWidth, cardWidth);
  const int secondW = widthBy(cst::kSecondColumnWidth, cardWidth);
  const int thirdW = widthBy(cst::kThirdColumnWidth, cardWidth);

  // Third column rectangle
  const QRect thirdRect(cardRect.left() + firstW + secondW + sideMargin,
                        cardRect.top(), thirdW - sideMargin * 2, cardHeight);

  painter->setFont(option.font);
  painter->setPen(option.palette.color(QPalette::Text));

  const QDate lastDate = QDate::currentDate();
  const QString dateStr =
      lastDate.isValid() ? lastDate.toString("dd.MM.yyyy") : "-";

  // Center text in the third column
  painter->drawText(thirdRect, Qt::AlignCenter, dateStr);
}

// ---------------- Fourth column (Status chip) ----------------
void QClientDelegate::drawStatusChip(QPainter *painter,
                                     const QStyleOptionViewItem &option,
                                     const ObxClient &client) {
  const QRect cardRect = option.rect;
  const int cardWidth = cardRect.width();
  const int cardHeight = cardRect.height();
  constexpr int sideMargin = cst::kSideMargin;

  const int firstW = widthBy(cst::kFirstColumnWidth, cardWidth);
  const int secondW = widthBy(cst::kSecondColumnWidth, cardWidth);
  const int thirdW = widthBy(cst::kThirdColumnWidth, cardWidth);
  const int fourthW = widthBy(cst::kFourthColumnWidth, cardWidth);

  // Fourth column rectangle
  const QRect fourthRect(cardRect.left() + firstW + secondW + thirdW +
                             sideMargin,
                         cardRect.top(), fourthW - sideMargin * 2, cardHeight);

  const QString chipText = client.client_active ? "Активен" : "Неактивен";
  const QColor chipColor =
      client.client_active ? QColor(0, 170, 120) : QColor(160, 160, 160);

  // Center the chip inside the fourth column
  constexpr int chipW = cst::kChipWidth;
  constexpr int chipH = cst::kChipHeight;

  const QRect chipRect(fourthRect.center().x() - chipW / 2,
                       fourthRect.center().y() - chipH / 2, chipW, chipH);

  painter->setBrush(chipColor);
  painter->setPen(Qt::NoPen);
  painter->drawRoundedRect(chipRect, 10, 10);

  painter->setPen(Qt::white);
  painter->drawText(chipRect, Qt::AlignCenter, chipText);
}

// ---------------- Fifth column (Action buttons) ----------------
void QClientDelegate::drawActions(QPainter *painter,
                                         const QStyleOptionViewItem &option,
                                         const ObxClient & /*client*/) {
  const auto [btn1Rect, btn2Rect] = calculateButtonRects(option);

  painter->setBrush(QColor(200, 200, 200));
  painter->setPen(Qt::NoPen);

  painter->drawEllipse(btn1Rect);
  painter->drawEllipse(btn2Rect);

  painter->setPen(Qt::black);
  // Edit button
  painter->drawText(btn1Rect, Qt::AlignCenter, "E");
  // Delete button
  painter->drawText(btn2Rect, Qt::AlignCenter, "D");
}

std::pair<QRect, QRect>
QClientDelegate::calculateButtonRects(const QStyleOptionViewItem &option) {
  const QRect cardRect = option.rect;
  const int cardWidth = cardRect.width();
  const int cardHeight = cardRect.height();
  constexpr int sideMargin = cst::kSideMargin;

  const int firstW = widthBy(cst::kFirstColumnWidth, cardWidth);
  const int secondW = widthBy(cst::kSecondColumnWidth, cardWidth);
  const int thirdW = widthBy(cst::kThirdColumnWidth, cardWidth);
  const int fourthW = widthBy(cst::kFourthColumnWidth, cardWidth);
  const int fifthW = widthBy(cst::kFifthColumnWidth, cardWidth);

  // Fifth column rectangle
  const QRect fifthRect(cardRect.left() + firstW + secondW + thirdW + fourthW +
                            sideMargin,
                        cardRect.top(), fifthW - sideMargin * 2, cardHeight);

  // Two action buttons aligned to the right
  constexpr int btnSize = cst::kActionBtnSize;
  constexpr int btnMargin = cst::kActionBtnMargin;

  const QRect btn2Rect(fifthRect.right() - btnSize,
                       fifthRect.center().y() - btnSize / 2, btnSize, btnSize);

  const QRect btn1Rect(btn2Rect.left() - btnMargin - btnSize,
                       fifthRect.center().y() - btnSize / 2, btnSize, btnSize);

  return {btn1Rect, btn2Rect};
}

bool QClientDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) {
  const auto [button1Rect, button2Rect] = calculateButtonRects(option);

  const auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);
  if (const auto pos = mouseEvent->pos();
      event->type() == QEvent::MouseButtonPress) {
    if (button1Rect.contains(pos)) {
      // Edit button clicked
      emit displayButtonClicked(index);
      return true;
    } else if (button2Rect.contains(pos)) {
      // Delete button clicked
      emit removeButtonClicked(index);
      return true;
    }
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// ---------------- Helper ----------------
int QClientDelegate::countAge(const QDate &birthDate) {
  const QDate now = QDate::currentDate();
  int age = now.year() - birthDate.year();
  if (now.month() < birthDate.month() ||
      (now.month() == birthDate.month() && now.day() < birthDate.day())) {
    age--;
  }
  return age;
}
