#include "main_window.h"
#include "../widgets/app_settings.h"
#include "ui/app/ui_mainwindow.h"

#include <oclero/qlementine/widgets/AboutDialog.hpp>

#include <QApplication>
#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>

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

  auto *titleLayout = new QHBoxLayout();
  titleLayout->setContentsMargins(0, 0, 0, 0);
  titleLayout->setSpacing(10);
  auto *titleIconLabel = new QLabel(this);
  titleIconLabel->setPixmap(QIcon(":/icons/brain-solid-full.svg").pixmap(24, 24));
  auto *titleTextLabel = new QLabel(mUi->label->text(), this);
  titleTextLabel->setFont(mUi->label->font());
  auto *titleWidget = new QWidget(this);
  titleWidget->setLayout(titleLayout);
  titleLayout->addWidget(titleIconLabel);
  titleLayout->addWidget(titleTextLabel);
  titleLayout->addStretch();
  mUi->gridLayout->replaceWidget(mUi->label, titleWidget);
  mUi->label->hide();
  mUi->label->deleteLater();

  // Create navigation buttons
  mBtnCalendar =
      new TabButton(QIcon(":/icons/calendar-solid-full.svg"), tr(": NAV_CALENDAR"), this);
  mBtnClients =
      new TabButton(QIcon(":/icons/users-line-solid-full.svg"), tr(": NAV_CLIENTS"), this);
  mBtnProfile =
      new TabButton(QIcon(":/icons/users-gear-solid-full.svg"), tr(": NAV_DETAILS"), this);

  // Add buttons to the vertical layout
  mUi->verticalLayout->addWidget(mBtnCalendar);
  mUi->verticalLayout->addWidget(mBtnClients);
  mUi->verticalLayout->addWidget(mBtnProfile);

  // Add stretch to push buttons to the top
  mUi->verticalLayout->addStretch();

  // Initialize default UI style
  initDefaultStyle();
  setupMenuBar();
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

  if (pcm::app_settings::showStatusBarMessages()) {
    statusBar()->showMessage(tr("Opened %1").arg(pageTitle(page)), 2000);
  } else {
    statusBar()->clearMessage();
  }
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

void MainWindow::setupMenuBar() {
  auto *appMenu = menuBar()->addMenu(tr("&Application"));

  mSettingsAction =
      appMenu->addAction(QIcon(":/icons/users-gear-solid-full.svg"), tr("Settings"));
  mAboutAction =
      appMenu->addAction(QIcon(":/icons/brain-solid-full.svg"), tr("About"));
  appMenu->addSeparator();
  mQuitAction = appMenu->addAction(tr("Quit"));

  connect(mSettingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);
  connect(mAboutAction, &QAction::triggered, this, &MainWindow::openAboutDialog);
  connect(mQuitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::openSettingsDialog() {
  SettingsDialog dialog(this);
  dialog.exec();
  refreshPageAppearance();
}

void MainWindow::openAboutDialog() {
  oclero::qlementine::AboutDialog dialog(this);
  dialog.setWindowTitle(tr("About"));
  dialog.setIcon(QIcon(":/icons/brain-solid-full.svg"));
  dialog.setApplicationName(QApplication::applicationDisplayName());
  dialog.setApplicationVersion(QApplication::applicationVersion());
  dialog.setDescription(
      tr("Desktop workspace for calendar scheduling, client management, and session tracking."));
  dialog.setLicense(tr("Built with Qt, DuckDB, and Qlementine."));
  dialog.setCopyright(QStringLiteral("2026 PsyClientManager"));
  dialog.exec();
}

QString MainWindow::pageTitle(const Pages page) const {
  switch (page) {
    case Pages::clientInfo:
      return tr("Clients");
    case Pages::eventInfo:
      return tr("Calendar");
    case Pages::clientCard:
      return tr("Details");
  }

  return tr("Page");
}

void MainWindow::refreshPageAppearance() {
  if (const auto eventPage =
          dynamic_cast<QEventInfoPage *>(mPages.value(Pages::eventInfo, nullptr))) {
    eventPage->refreshAppearance();
  }
}


MainWindow::~MainWindow() = default;
