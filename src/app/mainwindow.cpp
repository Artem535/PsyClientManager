#include "mainwindow.h"
#include "clientinfo.h"
#include "ui/app/ui_mainwindow.h"
#include <memory>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()) {
  m_ui->setupUi(this);
}

void MainWindow::add_client_info_page(std::shared_ptr<ClientModel> model) {
  m_ui->tab_widget->addTab(new EventInfo(this), "Event info");
  m_ui->tab_widget->addTab(new ClientInfo(model, this), "Client info");
}

MainWindow::~MainWindow() = default;
