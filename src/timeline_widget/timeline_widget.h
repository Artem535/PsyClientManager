// src/timeline_widget/timeline_widget.h
#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QVBoxLayout>
#include <QWidget>
#include <QDate>
#include <QPointer>

#include "event_item.h"
#include "database.h"
#include "event_view.h"
#include "qtimeline_model.h"


class QTimelineWidget final : public QWidget {
    Q_OBJECT

public:
    explicit QTimelineWidget(const std::shared_ptr<pcm::database::Database> &db, QWidget *parent = nullptr);

  QTimelineWidget(QTimelineModel *model, QWidget *parent);
  ~QTimelineWidget() override;

public slots:
    void onSelectedDayChanged(const QDate &date) const;

    // Возвращаем ID события, если нужно
    [[nodiscard]] obx_id addEvent(const ObxEvent &event) const;


    // Сигнал для обновления сцены (если нужно вручную)
    void updateScene();

    void updateEvent(const ObxEvent &event) const;

signals:
    void eventSelected(QEventItem *event);

    void needSceneUpdate();

private:
    QVBoxLayout *mLayout = nullptr;
    QEventView *mEventView = nullptr;
    QTimelineModel *mModel = nullptr; // вместо QEventDataManager
};
