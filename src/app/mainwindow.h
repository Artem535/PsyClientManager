#pragma once

#include "clientinfo.h"
#include "eventinfo.h"
#include "database.h"
#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  void setDatabase(std::shared_ptr<pcm::database::Database> db);
  void add_client_info_page(std::shared_ptr<QClientModel> model);
  void add_event_info_page(std::shared_ptr<pcm::database::Database> db);
  
private:
  std::unique_ptr<Ui::MainWindow> mUi;
  std::shared_ptr<pcm::database::Database> mDb{nullptr};
};
