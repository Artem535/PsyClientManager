#pragma once

#include "event_item.h"

#include <QComboBox>
#include <QDate>
#include <QHash>
#include <QPointer>
#include <QWidget>
#include <memory>

// Forward declaration for UI
namespace Ui {
class EventDetails;
}

/**
 * @brief A widget for displaying and editing event details.
 * Contains all input fields and buttons for creating/modifying an event.
 * Does not own the database; receives client data externally.
 */
class QEventDetailsWidget final : public QWidget {
  Q_OBJECT

public:
  explicit QEventDetailsWidget(QWidget *parent = nullptr);
  ~QEventDetailsWidget() override;

  /**
   * @brief Loads data from an existing event into the form for editing.
   * @param event Pointer to the event.
   */
  void loadEvent(QEventItem *event,
                 std::optional<int64_t> clientId = std::nullopt);

  /**
   * @brief Loads data from an existing event and enters edit mode.
   */
  void startEditingEvent(QEventItem *event,
                         std::optional<int64_t> clientId = std::nullopt);

  /**
   * @brief Clears the form and switches to "create new event" mode.
   */
  void startCreatingNewEvent(const QDate &date = QDate::currentDate());

  /**
   * @brief Configures widget for usage inside a modal dialog.
   */
  void setDialogMode(bool enabled);

  /**
   * @brief Checks if the widget is in edit mode.
   * @return true if in edit mode, false otherwise.
   */
  [[nodiscard]] bool isInEditMode() const;

  /**
   * @brief Checks if a new event is being created.
   * @return true if creating a new event, false otherwise.
   */
  [[nodiscard]] bool isCreatingNewEvent() const;

  /**
   * @brief Returns a pointer to the current event (may be nullptr if creating a
   * new one).
   * @return Pointer to the event.
   */
  [[nodiscard]] QEventItem *currentEvent() const;

  /**
   * @brief Sets the list of clients available for selection.
   * @param clients A hash map where key is client ID and value is client
   * display name.
   */
  void setClientList(const QHash<int64_t, QString> &clients);

signals:
  void provideClientEventPairSave(int64_t clientId, int64_t eventId);

  /**
   * @brief Signal emitted when user requests to save the event.
   * Passes a pointer to the event data that should be saved.
   */
  void provideEventSave(QEventItem *event);

  /**
   * @brief Signal emitted when user cancels editing/creation.
   */
  void provideEditingCanceled();

  /**
   * @brief Signal emitted when the edit mode changes.
   */
  void provideEditModeChanged();

  void provideFillClientComboBox(QComboBox *combobox);

private slots:
  // --- Button Slots ---
  void onApplyClicked();
  void onCancelClicked();
  void onAddClicked();
  void onChangeClicked();

  // --- Input Change Slots ---
  void onEventTypeToggled(bool checked);
  void onTimeFromChanged(const QDateTime &dt);
  void onTimeToChanged(const QDateTime &dt);

private:
  // --- Initialization ---
  void initUi();
  void initConnections();
  void initDefaultStyle();
  void initEditStyle();
  void initDefaultStates() const;
  void initDefaultTimes() const;
  void updateButtonState() const;

  // --- Validation & Data Collection ---
  bool validateInput();
  [[nodiscard]] ObxEvent collectEventData() const;

  // --- UI ---
  std::unique_ptr<Ui::EventDetails> mUI;

  // --- Data ---
  QPointer<QEventItem> mCurrentEvent;
  QHash<int64_t, QString> mClientList;
  bool mInEditMode = false;
  bool mCreatingNewEvent = false;
  bool mDialogMode = false;
};
