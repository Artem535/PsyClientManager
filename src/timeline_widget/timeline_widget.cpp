// src/timeline_widget/timeline_widget.cpp
#include "timeline_widget.h"
#include "qtimeline_model.h"

Q_LOGGING_CATEGORY(logTimelineWidget, "pcm.timeline")

QTimelineWidget::QTimelineWidget(const std::shared_ptr<pcm::database::Database> &db, QWidget *parent)
    : QWidget(parent) {
    mLayout = new QVBoxLayout(this);
    setLayout(mLayout);

    // Создаём модель
    mModel = new QTimelineModel(db, this);

    // Создаём view
    mEventView = new QEventView(this);
    mEventView->setModel(mModel); // <-- ключевое: передаём модель

    mLayout->addWidget(mEventView);

    // Подключаем сигналы
    connect(mModel, &QTimelineModel::eventsLoaded, this, &QTimelineWidget::updateScene);
    connect(mEventView, &QEventView::eventSelected, this, &QTimelineWidget::eventSelected);

    qCDebug(logTimelineWidget) << "QTimelineWidget initialized with QTimelineModel";
}

QTimelineWidget::~QTimelineWidget() = default;

void QTimelineWidget::onSelectedDayChanged(const QDate &date) const {
    qCDebug(logTimelineWidget) << "QTimelineWidget::onSelectedDayChanged | Date:" << date;

    // Модель сама загружает данные из БД
    mModel->loadEventsForDay(date);
}

obx_id QTimelineWidget::addEvent(const ObxEvent &event) const {
    if (!mModel) {
        qCWarning(logTimelineWidget) << "QTimelineWidget::addEvent | Model is null";
        return 0;
    }

    qCDebug(logTimelineWidget) << "QTimelineWidget::addEvent | Adding event:" << event.name.c_str();

    // Модель сама добавит в БД и в вектор
    return mModel->addEvent(event);
}

void QTimelineWidget::updateScene() {
    emit needSceneUpdate();

}

void QTimelineWidget::updateEvent(const ObxEvent &event) const {
    if (!mModel) return;
    mModel->updateEvent(event);
}
