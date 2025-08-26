#pragma once
// === Qt ===
#include <QAbstractItemModel>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QWidget>

// === STL ===
#include <memory>

// === Local ===
#include "database.h"
#include "eventitem.h"
#include "eventview.h"
#include "timelinewidget.h"

namespace Ui {
class EventInfo;
}

/**
 * @brief The QEventInfoPage class represents a page for displaying and managing
 * event information.
 *
 * This class provides functionality for viewing, editing, and adding new
 * events, as well as interacting with the calendar and timeline widgets.
 */
class QEventInfoPage final : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Constructs a QEventInfoPage object.
   *
   * Initializes the UI, connects signals and slots, and sets up default time
   * values.
   *
   * @param db A shared pointer to the database.
   * @param parent The parent widget (optional).
   */
  explicit QEventInfoPage(const std::shared_ptr<pcm::database::Database> &db,
                          QWidget *parent = nullptr);

  /** @brief Destructor. */
  ~QEventInfoPage() override;

signals:
  /**
   * @brief Emitted when the edit mode is changed.
   */
  void changedEditMode();

  /**
   * @brief Emitted to request clearing of the UI.
   * @note Not used in the current implementation.
   */
  void cleanUi();

  /**
   * @brief Emitted when a new event needs to be added.
   * @param event A pointer to the new event.
   */
  void needAddNewEvent(QEventItem *event);

  /**
   * @brief Emitted to request an update of the timeline scene.
   */
  void needSceneUpdate();

private slots:
  /**
   * @brief Slot triggered when an event is clicked in the timeline.
   * @param event A pointer to the selected event.
   */
  void onEventClicked(QEventItem *event);

  /**
   * @brief Adds an event to the timeline after editing.
   * @param event A pointer to the event.
   */
  void addEvent(QEventItem *event) const;

  /**
   * @brief Clears all UI fields.
   */
  void clearUi() const;

private:
  std::shared_ptr<pcm::database::Database> mDB; ///< Pointer to the database.
  std::unique_ptr<Ui::EventInfo> mUi;           ///< UI manager.
  QEventItem *mCurrentEvent =
      nullptr; ///< Pointer to the currently selected event.
  QTimelineWidget *mTimelineWidget; ///< Timeline widget.
  bool mCreatedNewEvent =
      false; ///< Flag indicating whether a new event was created.
  bool mInEditMode = false; ///< Edit mode flag.

  /**
   * @brief Connects calendar widget signals to appropriate slots.
   */
  void connectCalendar();

  /**
   * @brief Connects timeline widget signals to appropriate slots.
   */
  void connectTimeline();

  /**
   * @brief Connects "Change" and "Add" button signals.
   */
  void connectButtons();

  /**
   * @brief Connects "Cancel", "Apply" buttons and edit mode change signals.
   */
  void connectButtonBox();

  /**
   * @brief Connects time editors to synchronize start and end times.
   */
  void connectTimeEditors();

  /**
   * @brief Connects the signal for updating the timeline scene.
   */
  void connectSceneUpdate();

  /**
   * @brief Sets default start and end times to the current time.
   */
  void initDefaultTimes() const;

  /**
   * @brief Placeholder for initializing the client combo box.
   * @note Not implemented yet.
   */
  void initClientComboBox() const;

  void connectEventTypes() const;

  void initDefaultSates();
};
