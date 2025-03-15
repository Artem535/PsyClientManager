#pragma once
#include "clientmodel.h"
#include "config.h"
#include "database.h"
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QDateTime>
#include <QListWidget>
#include <QWidget>
#include <memory>
#include <qtmetamacros.h>

namespace Ui {
class ClientInfo;
}

/**
 * @brief The ClientInfo class represents a widget for displaying and editing
 * client information.
 *
 * This class inherits from QWidget and provides methods for displaying and
 * editing client information.
 */
class ClientInfo : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Constructs a new ClientInfo object.
   *
   * @param model A shared pointer to the client model object.
   * @param parent The parent widget.
   */
  explicit ClientInfo(std::shared_ptr<ClientModel> model,
                      QWidget *parent = nullptr);

  /**
   * @brief Destroys the ClientInfo object.
   */
  ~ClientInfo() override;

public slots:
  /**
   * @brief Updates the client preview with the specified index.
   *
   * @param index The index of the client.
   */
  void update_client_preview(const QModelIndex &index);

  /**
   * @brief Clears the client preview.
   */
  void clear_client_preview();

  /**
   * @brief Updates the client information with the specified index.
   *
   * @param index The index of the client.
   */
  void update_client_info(const QModelIndex &index);

  /**
   * @brief Adds a new client.
   */
  void add_new_client();

signals:
  /**
   * @brief Emitted when the editing of client information is finished.
   */
  void end_edit();

private:
  std::unique_ptr<Ui::ClientInfo> m_ui; /**< The user interface object. */
  std::shared_ptr<ClientModel> m_client_model; /**< The client model object. */

  /**< Indicates whether the widget is in edit mode. */
  bool in_edit_mode = false;

  /**
   * @brief Changes the mode of the edit fields.
   *
   * @param enable Indicates whether the edit fields should be enabled.
   */
  void change_edit_fields_mode(bool enable = false);

  /**
   * @brief Connects the widgets.
   */
  void connect_widgets();

  /**
   * @brief Returns the client information from the user interface.
   *
   * @return The client information.
   */
  Client get_client_from_ui() const;

  /**
   * @brief Calculates the age based on the birthdate.
   *
   * @param birthdate The birthdate.
   * @return The age.
   */
  int count_age(const QDate &birthdate) const;
};
