#include "main_window.h"
#include "ui/app/ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), mUi(std::make_unique<Ui::MainWindow>()) {
  mUi->setupUi(this);
  mUi->gridLayout->setColumnStretch(0, 0);
  mUi->gridLayout->setColumnStretch(1, 1);
  mUi->gridLayout->setRowStretch(0, 0);
  mUi->gridLayout->setRowStretch(1, 1);
  mPageCustomWidgetLayout = new QHBoxLayout(mUi->pageCustomWidgetHost);
  mPageCustomWidgetLayout->setContentsMargins(0, 0, 0, 0);
  mPageCustomWidgetLayout->setSpacing(0);

  // Create navigation buttons
  mBtnCalendar = new TabButton(tr(": NAV_CALENDAR"), this);
  mBtnClients = new TabButton(tr(": NAV_CLIENTS"), this);
  mBtnProfile = new TabButton(tr(": NAV_DETAILS"), this);

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
  connect(mBtnCalendar, &QPushButton::clicked,
          [this]() { showPage(Pages::eventInfo, mBtnCalendar); });

  connect(mBtnClients, &QPushButton::clicked,
          [this]() { showPage(Pages::clientInfo, mBtnClients); });

  connect(mBtnProfile, &QPushButton::clicked,
          [this]() { showPage(Pages::clientCard, mBtnProfile); });

  // When a client is selected in the list, show its info in the client card page
  connect(clientInfoPage, &ClientInfo::displayButtonClicked, clientCardPage,
          &QClientInfoCardPage::setClientInfo);

  // Switch to the client card page after selecting a client
  connect(clientInfoPage, &ClientInfo::displayButtonClicked,
          [this]() { showPage(Pages::clientCard, mBtnProfile); });

  // Forward the save client signal from the client card to the main window
  connect(clientCardPage, &QClientInfoCardPage::provideSaveClient,
          [&](const auto &client) { emit provideSaveClient(client); });

  // Forward the client-event pair save signal from event info to main window
  connect(eventInfoPage, &QEventInfoPage::provideClientEventPairSave,
          [this](const int64_t clientId, const int64_t eventId) {
            emit provideClientEventPairSave(clientId, eventId);
          });

  showPage(Pages::eventInfo, mBtnCalendar);
}

QWidget *MainWindow::getPage(const Pages page) { return mPages[page]; }

void MainWindow::setPageCustomWidget(const Pages page, QWidget *widget) {
  if (widget != nullptr) {
    widget->setParent(mUi->pageCustomWidgetHost);
  }

  mPageCustomWidgets.insertOrAssign(page, widget);

  if (mCurrentPage == page) {
    applyPageCustomWidget(page);
  }
}

void MainWindow::initDefaultStyle() const {
  checkButton(mBtnCalendar);
}

void MainWindow::checkButton(QPushButton *btn) const {
  mBtnCalendar->setChecked(false);
  mBtnClients->setChecked(false);
  mBtnProfile->setChecked(false);
  btn->setChecked(true);
}

void MainWindow::showPage(const Pages page, QPushButton *btn) {
  if (!mPagesIndex.contains(page)) {
    return;
  }

  mUi->stackedWidget->setCurrentIndex(mPagesIndex[page]);
  mCurrentPage = page;
  applyPageCustomWidget(page);
  checkButton(btn);
}

void MainWindow::applyPageCustomWidget(const Pages page) {
  while (const auto item = mPageCustomWidgetLayout->takeAt(0)) {
    if (const auto widget = item->widget()) {
      widget->hide();
    }
    delete item;
  }

  if (const auto widget = mPageCustomWidgets.value(page, nullptr)) {
    widget->setParent(mUi->pageCustomWidgetHost);
    widget->show();
    mPageCustomWidgetLayout->addWidget(widget);
  }
}


MainWindow::~MainWindow() = default;
