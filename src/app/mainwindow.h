#pragma once

#include "clientinfo.h"
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
  explicit MainWindow(std::shared_ptr<pcm::database::Database> db,
                      QWidget *parent = nullptr);

  ~MainWindow() override;

private:
  std::unique_ptr<Ui::MainWindow> m_ui;
};
