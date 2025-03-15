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

namespace Ui {
class ClientInfo;
}

class ClientInfo : public QWidget {
  Q_OBJECT

public:
  explicit ClientInfo(std::shared_ptr<ClientModel> model,
                      QWidget *parent = nullptr);
  ~ClientInfo() override;

public slots:
  void update_client_preview(const QModelIndex &index);
  void clear_client_preview();
  void update_client_info(const QModelIndex &index);
  void add_new_client();

private:
  std::unique_ptr<Ui::ClientInfo> m_ui;
  std::shared_ptr<ClientModel> m_client_model;
  bool in_edit_mode = false;

  void change_edit_fields_mode(bool enable = false);
  void connect_widgets();
  Client get_client_from_ui() const;
};
