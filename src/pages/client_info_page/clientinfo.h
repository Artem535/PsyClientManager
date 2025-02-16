#pragma once
#include "config.h"
#include "database.h"
#include <QWidget>
#include <memory>

namespace Ui {
class ClientInfo;
}

class ClientInfo : public QWidget {
  Q_OBJECT

public:
  explicit ClientInfo(std::shared_ptr<pcm::database::Database> db,
                      QWidget *parent = nullptr);
  ~ClientInfo() override;

private:
  std::unique_ptr<Ui::ClientInfo> m_ui;
  std::shared_ptr<pcm::database::Database> m_db;
};
