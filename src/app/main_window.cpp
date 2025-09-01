#include "main_window.h"
#include "ui/app/ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), mUi(std::make_unique<Ui::MainWindow>()) {
  mUi->setupUi(this);
}

void MainWindow::add_client_info_page(std::shared_ptr<QClientModel> model) {
  mUi->tab_widget->addTab(new ClientInfo(std::move(model), this), "Client info");
}

void MainWindow::add_event_info_page(std::shared_ptr<pcm::database::Database> db) {
  if (db) {
    mUi->tab_widget->addTab(new QEventInfoPage(db, this), "Event info");
  }
}
void MainWindow::add_detail_client_info_page() {
  mUi->tab_widget->addTab(new QClientInfoCardPage(this), "Detail client info");
}

MainWindow::~MainWindow() = default;
