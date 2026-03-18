// src/timeline_widget/timeline_widget.cpp
#include "timeline_widget.h"
#include "qtimeline_model.h"

Q_LOGGING_CATEGORY(logTimelineWidget, "pcm.timeline")

QTimelineWidget::QTimelineWidget(QTimelineModel *model, QWidget *parent)
    : QWidget(parent), mModel(model) {
  mLayout = new QVBoxLayout(this);
  mLayout->setContentsMargins(0, 0, 0, 0);
  mLayout->setSpacing(0);
  setLayout(mLayout);

  mEventView = new QEventView(this);
  mEventView->setModel(mModel);

  mLayout->addWidget(mEventView);

  connect(mModel, &QTimelineModel::eventsLoaded, this,
          &QTimelineWidget::updateScene);
  connect(mEventView, &QEventView::eventSelected, this,
          &QTimelineWidget::eventSelected);
  connect(mEventView, &QEventView::eventEditRequested, this,
          &QTimelineWidget::eventEditRequested);
  connect(mEventView, &QEventView::eventDeleteRequested, this,
          &QTimelineWidget::eventDeleteRequested);
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

int64_t QTimelineWidget::addEvent(const DuckEvent &event) const {
  if (!mModel) {
    qCWarning(logTimelineWidget) << "QTimelineWidget::addEvent | Model is null";
    return 0;
  }

  qCDebug(logTimelineWidget) << "QTimelineWidget::addEvent | Adding event:"
                             << QString::fromStdString(event.name.value_or(""));

  return mModel->addEvent(event);
}

void QTimelineWidget::updateScene() { emit needSceneUpdate(); }

void QTimelineWidget::updateEvent(const DuckEvent &event) const {
  if (!mModel)
    return;
  mModel->updateEvent(event);
}

void QTimelineWidget::removeEvent(const int64_t id) const {
  if (!mModel)
    return;
  mModel->removeEvent(id);
}
