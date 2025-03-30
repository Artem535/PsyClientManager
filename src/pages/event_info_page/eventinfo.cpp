#include "eventinfo.h"
#include "eventview.h"
#include "timelinewidget.h"
#include "ui/pages/ui_eventinfo.h"
#include <memory>

EventInfo::EventInfo(QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()) {
  mUi->setupUi(this);
  mUi->list_view_v_layout->addWidget(new TimelineWidget(this));
}

EventInfo::~EventInfo() = default;

// void EventInfo::create_gantt_chart() {
//   m_view = std::make_unique<KDGantt::View>(this);
//   m_date_time_grid = std::make_unique<KDGantt::DateTimeGrid>();

//   m_date_time_grid->setScale(KDGantt::DateTimeGrid::ScaleHour);
//   m_date_time_grid->setDayWidth(2400);

//   auto start_date = QDateTime(QDate(2025, 3, 19), QTime(10, 0));
//   auto end_date = QDateTime(QDate(2025, 3, 19), QTime(11, 0));

//   m_view->setGrid(m_date_time_grid.get());

//   auto model = new QStandardItemModel(this);
//   model->appendRow(QList<QStandardItem *>()
//                    << new MyStandardItem(QString("Educate personel"))
//                    << new MyStandardItem(KDGantt::TypeTask)
//                    << new MyStandardItem(start_date)
//                    << new MyStandardItem(end_date) << new MyStandardItem(0));

//   m_view->setModel(model);
// }