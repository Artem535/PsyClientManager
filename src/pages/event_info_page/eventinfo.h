#pragma once
#include "config.h"
#include "database.h"
#include "timelinewidget.h"
#include <KDGanttConstraintModel>
#include <KDGanttDateTimeGrid>
#include <KDGanttView>
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QDateTime>
#include <QListWidget>
#include <QStandardItemModel>
#include <QWidget>
#include <memory>

namespace Ui {
class EventInfo;
}

class EventInfo : public QWidget {
  Q_OBJECT

public:
  explicit EventInfo(QWidget *parent = nullptr);

  ~EventInfo() override;

private:
  std::unique_ptr<Ui::EventInfo> mUi;
};
