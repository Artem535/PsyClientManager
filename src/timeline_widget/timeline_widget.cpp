// src/timeline_widget/timeline_widget.cpp
#include "timeline_widget.h"
#include "qtimeline_model.h"

Q_LOGGING_CATEGORY(logTimelineWidget, "pcm.timeline")

QTimelineWidget::QTimelineWidget(QTimelineModel *model, QWidget *parent)
    : QWidget(parent), mModel(model) {
  mLayout = new QVBoxLayout(this);
  setLayout(mLayout);

  mEventView = new QEventView(this);
  mEventView->setModel(mModel);

  mLayout->addWidget(mEventView);

  connect(mModel, &QTimelineModel::eventsLoaded, this,
          &QTimelineWidget::updateScene);
  connect(mEventView, &QEventView::eventSelected, this,
          &QTimelineWidget::eventSelected);
  // Defaults
  mModel->loadEventsForDay(QDate::currentDate());

  qCDebug(logTimelineWidget)
      << "QTimelineWidget initialized with QTimelineModel";
}

QTimelineWidget::~QTimelineWidget() = default;

void QTimelineWidget::onSelectedDayChanged(const QDate &date) const {
  qCDebug(logTimelineWidget)
      << "QTimelineWidget::onSelectedDayChanged | Date:" << date;

  mModel->loadEventsForDay(date);
}

obx_id QTimelineWidget::addEvent(const ObxEvent &event) const {
  if (!mModel) {
    qCWarning(logTimelineWidget) << "QTimelineWidget::addEvent | Model is null";
    return 0;
  }

  qCDebug(logTimelineWidget)
      << "QTimelineWidget::addEvent | Adding event:" << event.name.c_str();

  return mModel->addEvent(event);
}

void QTimelineWidget::updateScene() { emit needSceneUpdate(); }

void QTimelineWidget::updateEvent(const ObxEvent &event) const {
  if (!mModel)
    return;
  mModel->updateEvent(event);
}
