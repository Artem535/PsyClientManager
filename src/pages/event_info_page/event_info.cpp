#include "event_info.h"
#include "ui/pages/ui_eventinfo.h"

Q_LOGGING_CATEGORY(logEventInfo, "pcm.EventInfo")

QEventInfoPage::QEventInfoPage(
    const std::shared_ptr<pcm::database::Database> &db, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>().release()), mDb(db) {
    mUi->setupUi(this);

    // Создаем TimelineWidget с моделью (без QEventDataManager!)
    mTimelineWidget = new QTimelineWidget(mDb, this);
    mUi->list_view_v_layout->addWidget(mTimelineWidget);

    // Подключаем сигналы
    connectCalendar();
    connectTimeline();
    connectButtons();
    connectButtonBox();        // <-- Здесь теперь вызывается saveEvent()
    connectTimeEditors();
    connectSceneUpdate();
    connectEventTypes();

    // Инициализация
    initDefaultTimes();
    initClientComboBox();
    initDefaultSates();

    // Обновляем состояние кнопок при смене режима
    connect(this, &QEventInfoPage::changedEditMode,
            [this]() { mUi->mEventType->setEnabled(mInEditMode); });
    updateButtonState();
}

// --- Подключение сигналов ---

void QEventInfoPage::connectCalendar() {
    connect(mUi->calendar_widget, &QCalendarWidget::clicked, mTimelineWidget,
            &QTimelineWidget::onSelectedDayChanged);
    // Обновляем дату события при смене календаря
    connect(mUi->calendar_widget, &QCalendarWidget::clicked,
            [this](const QDate &date) {
                mUi->mEventDate->setDateTime(QDateTime(date, QTime::currentTime()));
            });
}

void QEventInfoPage::connectTimeline() {
    connect(mTimelineWidget, &QTimelineWidget::eventSelected, this,
            &QEventInfoPage::onEventClicked);
}

void QEventInfoPage::connectButtons() {
    // Кнопка "Изменить"
    connect(mUi->mChangeButton, &QPushButton::clicked, [this]() {
        if (!mCurrentEvent) return;
        mInEditMode = true;
        emit changedEditMode();
    });

    // Кнопка "Добавить"
    connect(mUi->mAddButton, &QPushButton::clicked, [this]() {
        mInEditMode = true;
        mCreatedNewEvent = true;

        const auto crtDateTime = QDateTime::currentDateTime().toLocalTime();
        mCurrentEvent = new QEventItem(0, "Новое событие", crtDateTime, crtDateTime.addSecs(3600)); // +1 час по умолчанию

        emit onEventClicked(mCurrentEvent);
        emit changedEditMode();
    });
}

void QEventInfoPage::connectButtonBox() {
    const auto cancelButton = mUi->mButtonBox->button(QDialogButtonBox::Cancel);
    const auto applyButton = mUi->mButtonBox->button(QDialogButtonBox::Apply);

    connect(cancelButton, &QPushButton::clicked, [this]() {
        mInEditMode = false;
        if (mCreatedNewEvent && mCurrentEvent) {
            delete mCurrentEvent.data();
            mCurrentEvent.clear();
        }
        mCreatedNewEvent = false;
        emit changedEditMode();
    });

    // ✅ Главное изменение: вызываем saveEvent() вместо лямбды
    connect(applyButton, &QPushButton::clicked, this, &QEventInfoPage::saveEvent);

    connect(this, &QEventInfoPage::changedEditMode, [this]() {
        const bool inEdit = mInEditMode;
        mUi->mButtonBox->setVisible(inEdit);
        mUi->mChangeButton->setVisible(!inEdit);
        mUi->mAddButton->setVisible(!inEdit);
    });
}

void QEventInfoPage::connectTimeEditors() {
    connect(mUi->mTimeFrom, &QDateTimeEdit::dateTimeChanged,
            [this](const QDateTime &dt) {
                if (mUi->mTimeTo->dateTime() < dt) {
                    mUi->mTimeTo->setDateTime(dt.addSecs(60)); // Минимум 1 минута
                }
                updateButtonState();
            });
    connect(mUi->mTimeTo, &QDateTimeEdit::dateTimeChanged,
            [this](const QDateTime &dt) {
                if (dt < mUi->mTimeFrom->dateTime()) {
                    mUi->mTimeFrom->setDateTime(dt.addSecs(-60));
                }
                updateButtonState();
            });
}

void QEventInfoPage::connectSceneUpdate() {
    // connect(this, &QEventInfoPage::needSceneUpdate, mTimelineWidget,
    //         &QTimelineWidget::updateScene);
}

void QEventInfoPage::connectEventTypes() const {
    connect(mUi->mEventType, &QCheckBox::toggled, [this](const bool checked) {
        mUi->mEventType->setText(checked ? "Рабочее событие" : "Обычное событие");
        mUi->mClientComboBox->setVisible(checked);
        mUi->mClientComboxBoxLabel->setVisible(checked);
    });
}

// --- Инициализация ---

void QEventInfoPage::initDefaultTimes() const {
    const auto crtDateTime = QDateTime::currentDateTime();
    mUi->mEventDate->setDateTime(crtDateTime);
    mUi->mTimeFrom->setDateTime(crtDateTime);
    mUi->mTimeTo->setDateTime(crtDateTime.addSecs(3600)); // +1 час
}

void QEventInfoPage::initClientComboBox() const {
    mUi->mClientComboBox->clear();
    for (const auto &client : mDb->get_clients()) {
        QString clientName = "%1 %2";
        const auto title =
            clientName.arg(QString::fromStdString(client->name),
                           QString::fromStdString(client->last_name));
        mUi->mClientComboBox->addItem(title, QVariant::fromValue(client->id));
    }
}

void QEventInfoPage::initDefaultSates() {
    mUi->mClientComboBox->setVisible(false);
    mUi->mClientComboxBoxLabel->setVisible(false);
    mUi->mEventType->setEnabled(false);
    emit changedEditMode();
}

// --- Основная логика ---

void QEventInfoPage::onEventClicked(QEventItem *event) {
    if (!event) return;

    qCDebug(logEventInfo) << "EventInfoPage::onEventClicked|"
                          << "Start time:" << event->getStartTime()
                          << "End time:" << event->getEndTime()
                          << "ID:" << event->getId();

    mUi->mTitle->setText(event->getTitle());
    mUi->mTimeFrom->setDateTime(event->getStartTime());
    mUi->mTimeTo->setDateTime(event->getEndTime());
    const bool isWorkItem = event->isWorkItem();
    mUi->mEventType->setChecked(isWorkItem);
    mCurrentEvent = event;

    // Если событие рабочее — подгружаем клиента
    if (isWorkItem && event->getId() != 0) {
        try {
            const auto client = mDb->get_client_by_event(event->getId());
            const auto clientId = QVariant::fromValue(client.id);
            if (const int index = mUi->mClientComboBox->findData(clientId);
                index != -1) {
                mUi->mClientComboBox->setCurrentIndex(index);
            }
        } catch (const std::exception &e) {
            qCWarning(logEventInfo) << "Не удалось загрузить клиента для события:" << e.what();
        }
    }

    updateButtonState();
}

// ✅ Новый метод: сохранение события
void QEventInfoPage::saveEvent() {
    if (!validateInput()) {
        return;
    }

    ObxEvent event;
    event.name = mUi->mTitle->text().toStdString();
    event.start_date = QDateTime(mUi->mEventDate->date(), mUi->mTimeFrom->time()).toMSecsSinceEpoch();
    event.end_date = QDateTime(mUi->mEventDate->date(), mUi->mTimeTo->time()).toMSecsSinceEpoch();
    event.is_work_event = mUi->mEventType->isChecked();
    event.duration = (event.end_date - event.start_date) / 1000; // в секундах

    obx_id eventId = 0;

    if (mCreatedNewEvent || !mCurrentEvent || mCurrentEvent->getId() == 0) {
        // Создаем новое событие
        eventId = mTimelineWidget->addEvent(event);
    } else {
        // Обновляем существующее
        event.id = mCurrentEvent->getId();
        mTimelineWidget->updateEvent(event); // ← нужно реализовать в QTimelineWidget
        eventId = event.id;
    }

    // Привязываем клиента, если событие рабочее
    if (event.is_work_event && eventId != 0) {
        obx_id clientId = mUi->mClientComboBox->currentData().toULongLong();
        if (clientId != 0) {
            [[maybe_unused]] auto dbId = mDb->add_event_client(eventId, clientId);
        }
    }

    // Выходим из режима редактирования
    mInEditMode = false;
    mCreatedNewEvent = false;
    emit changedEditMode();
    emit needSceneUpdate();
}

// ✅ Вспомогательный метод: валидация
bool QEventInfoPage::validateInput() {
    const bool isTitleEmpty = mUi->mTitle->text().trimmed().isEmpty();
    const bool isZeroDuration =
        mUi->mTimeTo->dateTime() <= mUi->mTimeFrom->dateTime();

    if (isTitleEmpty) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Название события не может быть пустым"));
        return false;
    }
    if (isZeroDuration) {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Продолжительность события должна быть больше нуля"));
        return false;
    }
    return true;
}

// ✅ Обновление состояния кнопки Apply
void QEventInfoPage::updateButtonState() const {
    const bool isValid = !mUi->mTitle->text().trimmed().isEmpty() &&
                         mUi->mTimeTo->dateTime() > mUi->mTimeFrom->dateTime();
    if (const auto applyButton = mUi->mButtonBox->button(QDialogButtonBox::Apply)) {
        applyButton->setEnabled(isValid);
    }
}

QEventInfoPage::~QEventInfoPage() = default;