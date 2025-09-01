#include "main_window.h"
#include "ui/app/ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), mUi(std::make_unique<Ui::MainWindow>()) {
  mUi->setupUi(this);
}

void MainWindow::addClientInfoPage(std::shared_ptr<QClientModel> model) {
  mUi->tab_widget->addTab(new ClientInfo(std::move(model), this), "Client info");
}

void MainWindow::addEventInfoPage(std::shared_ptr<pcm::database::Database> db) {
  if (db) {
    mUi->tab_widget->addTab(new QEventInfoPage(db, this), "Event info");
  }
}
void MainWindow::addClientCardPage() {
  mUi->tab_widget->addTab(new QClientInfoCardPage(this), "Detail client info");
}

MainWindow::~MainWindow() = default;
