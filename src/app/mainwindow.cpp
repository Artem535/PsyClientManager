#include "mainwindow.h"
#include "clientinfo.h"
#include "ui/app/ui_mainwindow.h"
#include <memory>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), mUi(std::make_unique<Ui::MainWindow>()) {
  mUi->setupUi(this);
}

void MainWindow::add_client_info_page(std::shared_ptr<ClientModel> model) {
  mUi->tab_widget->addTab(new ClientInfo(model, this), "Client info");
}

void MainWindow::add_event_info_page(std::shared_ptr<pcm::database::Database> db) {
  if (db) {
    mUi->tab_widget->addTab(new EventInfoPage(db, this), "Event info");
  }
}

MainWindow::~MainWindow() = default;
