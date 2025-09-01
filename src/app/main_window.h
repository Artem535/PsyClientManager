#pragma once

#include "client_info.h"
#include "event_info.h"
#include "database.h"
#include "client_info_card.h"
#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  void setDatabase(std::shared_ptr<pcm::database::Database> db);
  void addClientInfoPage(std::shared_ptr<QClientModel> model);
  void addEventInfoPage(std::shared_ptr<pcm::database::Database> db);
  void addClientCardPage();
  
private:
  std::unique_ptr<Ui::MainWindow> mUi;
  std::shared_ptr<pcm::database::Database> mDb{nullptr};
};
