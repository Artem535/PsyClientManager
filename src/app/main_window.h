#pragma once

#include "client_info.h"
#include "event_info.h"
#include "database.h"
#include "client_info_card.h"
#include "tab_button.h"

#include <QMainWindow>
#include <QLabel>

#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief Main application window class.
 *
 * Manages the main UI and switching between different pages like
 * client info, event info, and client card.
 */
class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  /**
   * @brief Enum to identify the available pages in the application.
   */
  enum class Pages { clientInfo, eventInfo, clientCard };

  /**
   * @brief Constructor for the MainWindow class.
   * Initializes the UI and navigation buttons.
   */
  explicit MainWindow(QWidget *parent = nullptr);

  /** @brief Destructor. */
  ~MainWindow() override;

  /**
   * @brief Adds the client information page to the application.
   * @param model Shared pointer to the client model.
   */
  void addClientInfoPage(std::shared_ptr<QClientModel> model);

  /**
   * @brief Adds the event information page to the application.
   * @param model Pointer to the timeline model.
   */
  void addEventInfoPage(QTimelineModel *model);

  /**
   * @brief Adds the client card (details) page to the application.
   */
  void addClientCardPage();

  /**
   * @brief Sets up all signal/slot connections between UI elements and logic.
   */
  void connectSignals();

  /**
   * @brief Returns a pointer to the specified page widget.
   * @param page The page enum value.
   * @return QWidget* Pointer to the corresponding page.
   */
  QWidget* getPage(Pages page);

signals:
  /**
   * @brief Emitted when a client should be saved.
   * @param client The client data to save.
   */
  void provideSaveClient(const ObxClient &client);

  /**
   * @brief Emitted when a client-event association should be saved.
   * @param clientId ID of the client.
   * @param eventId ID of the event.
   */
  void provideClientEventPairSave(const int64_t clientId, const int64_t eventId);

private:
  // Map of pages by type
  QHash<Pages, QWidget*> mPages;
  QHash<Pages, int> mPagesIndex;

  // UI manager
  std::unique_ptr<Ui::MainWindow> mUi;

  // Database connection (currently unused)
  std::shared_ptr<pcm::database::Database> mDb{nullptr};

  // Navigation buttons
  TabButton *mBtnCalendar{nullptr};
  TabButton *mBtnClients{nullptr};
  TabButton *mBtnProfile{nullptr};

  /**
   * @brief Initializes default UI style (colors, fonts, etc.).
   */
  void initDefaultStyle() const;

  /**
   * @brief Highlights the currently active navigation button.
   * @param btn The button to mark as active.
   */
  void checkButton(QPushButton *btn) const;
};