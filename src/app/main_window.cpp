#include "main_window.h"
#include "ui/app/ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), mUi(std::make_unique<Ui::MainWindow>()) {
  mUi->setupUi(this);
}

void MainWindow::addClientInfoPage(std::shared_ptr<QClientModel> model) {
  const auto page = new ClientInfo(std::move(model), this);
  mPages.insertOrAssign(Pages::clientInfo, page);

  const int index = mUi->tab_widget->addTab(page, "Client info");
  mPagesIndex.insertOrAssign(Pages::clientInfo, index);
}

void MainWindow::addEventInfoPage(std::shared_ptr<pcm::database::Database> db) {
  const auto page = new QEventInfoPage(db, this);
  mPages.insertOrAssign(Pages::eventInfo, page);

  const int index = mUi->tab_widget->addTab(page, "Event info");
  mPagesIndex.insertOrAssign(Pages::eventInfo, index);
}
void MainWindow::addClientCardPage() {
  const auto page = new QClientInfoCardPage(this);
  mPages.insertOrAssign(Pages::clientCard, page);

  const int index = mUi->tab_widget->addTab(page, "Detail client info");
  mPagesIndex.insertOrAssign(Pages::clientCard, index);
}
void MainWindow::connectSignals() {
  const auto clientInfoPage =
      dynamic_cast<ClientInfo *>(mPages[Pages::clientInfo]);
  const auto clientCardPage =
      dynamic_cast<QClientInfoCardPage *>(mPages[Pages::clientCard]);

  connect(clientInfoPage, &ClientInfo::displayButtonClicked, clientCardPage,
          &QClientInfoCardPage::setClientInfo);
  connect(clientInfoPage, &ClientInfo::displayButtonClicked, [&]() {
    mUi->tab_widget->setCurrentIndex(mPagesIndex[Pages::clientCard]);
  });

  connect(clientCardPage, &QClientInfoCardPage::provideSaveClient, [&](const auto &client) {
    emit provideSaveClient(client);
  });
}

MainWindow::~MainWindow() = default;
