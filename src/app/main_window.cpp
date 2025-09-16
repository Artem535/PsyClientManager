#include "main_window.h"
#include "ui/app/ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), mUi(std::make_unique<Ui::MainWindow>()) {
  mUi->setupUi(this);

  // Create navigation buttons
  mBtnCalendar = new TabButton("Calendar", this);
  mBtnClients = new TabButton("Clients", this);
  mBtnProfile = new TabButton("Details", this);

  // Add buttons to the vertical layout
  mUi->verticalLayout->addWidget(mBtnCalendar);
  mUi->verticalLayout->addWidget(mBtnClients);
  mUi->verticalLayout->addWidget(mBtnProfile);

  // Add stretch to push buttons to the top
  mUi->verticalLayout->addStretch();

  // Initialize default UI style
  initDefaultStyle();
}


void MainWindow::addClientInfoPage(std::shared_ptr<QClientModel> model) {
  const auto page = new ClientInfo(std::move(model), this);
  mPages.insertOrAssign(Pages::clientInfo, page);

  const int index = mUi->stackedWidget->addWidget(page);
  mPagesIndex.insertOrAssign(Pages::clientInfo, index);
}


void MainWindow::addEventInfoPage(QTimelineModel *model) {
  const auto page = new QEventInfoPage(model, this);
  mPages.insertOrAssign(Pages::eventInfo, page);

  const int index = mUi->stackedWidget->addWidget(page);
  mPagesIndex.insertOrAssign(Pages::eventInfo, index);
}


void MainWindow::addClientCardPage() {
  const auto page = new QClientInfoCardPage(this);
  mPages.insertOrAssign(Pages::clientCard, page);

  const int index = mUi->stackedWidget->addWidget(page);
  mPagesIndex.insertOrAssign(Pages::clientCard, index);
}


void MainWindow::connectSignals() {
  const auto clientInfoPage =
      dynamic_cast<ClientInfo *>(mPages[Pages::clientInfo]);
  const auto clientCardPage =
      dynamic_cast<QClientInfoCardPage *>(mPages[Pages::clientCard]);
  const auto eventInfoPage =
      dynamic_cast<QEventInfoPage *>(mPages[Pages::eventInfo]);

  // Connect navigation buttons to switch pages
  connect(mBtnCalendar, &QPushButton::clicked, [&]() {
    mUi->stackedWidget->setCurrentIndex(mPagesIndex[Pages::eventInfo]);
    checkButton(mBtnCalendar);
  });

  connect(mBtnClients, &QPushButton::clicked, [&]() {
    mUi->stackedWidget->setCurrentIndex(mPagesIndex[Pages::clientInfo]);
    checkButton(mBtnClients);
  });

  connect(mBtnProfile, &QPushButton::clicked, [&]() {
    mUi->stackedWidget->setCurrentIndex(mPagesIndex[Pages::clientCard]);
    checkButton(mBtnProfile);
  });

  // When a client is selected in the list, show its info in the client card page
  connect(clientInfoPage, &ClientInfo::displayButtonClicked, clientCardPage,
          &QClientInfoCardPage::setClientInfo);

  // Switch to the client card page after selecting a client
  connect(clientInfoPage, &ClientInfo::displayButtonClicked, [&]() {
    mUi->stackedWidget->setCurrentIndex(mPagesIndex[Pages::clientCard]);
    checkButton(mBtnProfile);
  });

  // Forward the save client signal from the client card to the main window
  connect(clientCardPage, &QClientInfoCardPage::provideSaveClient,
          [&](const auto &client) { emit provideSaveClient(client); });

  // Forward the client-event pair save signal from event info to main window
  connect(eventInfoPage, &QEventInfoPage::provideClientEventPairSave,
          [this](const obx_id clientId, const obx_id eventId) {
            emit provideClientEventPairSave(clientId, eventId);
          });
}

QWidget *MainWindow::getPage(const Pages page) { return mPages[page]; }

void MainWindow::initDefaultStyle() {}

void MainWindow::checkButton(QPushButton *btn) const {
  mBtnCalendar->setChecked(false);
  mBtnClients->setChecked(false);
  mBtnProfile->setChecked(false);
  btn->setChecked(true);
}


MainWindow::~MainWindow() = default;