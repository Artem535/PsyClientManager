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
  QTimelineWidget(QTimelineModel *model, QWidget *parent);
  ~QTimelineWidget() override;

public slots:
    void onSelectedDayChanged(const QDate &date) const;

    [[nodiscard]] int64_t addEvent(const DuckEvent &event, bool allowOverlap = true) const;

    void updateScene();

    void updateEvent(const DuckEvent &event, bool allowOverlap = true) const;
    void removeEvent(int64_t id) const;
    [[nodiscard]] bool hasConflict(const DuckEvent &event) const;
    const QVector<DuckEvent> &events() const;

signals:
    void eventSelected(QEventItem *event);
    void eventEditRequested(QEventItem *event);
    void eventDeleteRequested(int64_t eventId);

    void needSceneUpdate();

private:
    QVBoxLayout *mLayout = nullptr;
    QEventView *mEventView = nullptr;
    QTimelineModel *mModel = nullptr;
};
