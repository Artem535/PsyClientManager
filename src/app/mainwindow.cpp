#include "mainwindow.h"
#include "clientinfo.h"
#include "ui/app/ui_mainwindow.h"
#include <memory>

MainWindow::MainWindow(std::shared_ptr<pcm::database::Database> db,
                       QWidget *parent)
    : QMainWindow(parent), m_ui(std::make_unique<Ui::MainWindow>()) {
  m_ui->setupUi(this);
  m_ui->tab_widget->addTab(new ClientInfo(db), "Client info");
}

MainWindow::~MainWindow() = default;
