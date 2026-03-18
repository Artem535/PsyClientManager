#include "main_window.h"
#include "../widgets/app_settings.h"
#include "ui/app/ui_mainwindow.h"

#include <oclero/qlementine/widgets/AboutDialog.hpp>

#include <QApplication>
#include <QIcon>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), mUi(std::make_unique<Ui::MainWindow>()) {
  mUi->setupUi(this);
  menuBar()->hide();
  mUi->gridLayout->setColumnStretch(0, 0);
  mUi->gridLayout->setColumnStretch(1, 1);
  mUi->gridLayout->setRowStretch(0, 0);
  mUi->gridLayout->setRowStretch(1, 1);
  mPageCustomWidgetLayout = new QHBoxLayout(mUi->pageCustomWidgetHost);
  mPageCustomWidgetLayout->setContentsMargins(
      pcm::widgets::constants::kPageContentMargin +
          pcm::widgets::constants::kPanelPadding,
      0,
      pcm::widgets::constants::kPageContentMargin +
          pcm::widgets::constants::kPanelPadding,
      0);
  mPageCustomWidgetLayout->setSpacing(pcm::widgets::constants::kPanelPadding);

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
  setupUtilityButtons();

  // Initialize default UI style
  initDefaultStyle();
}


void MainWindow::addClientInfoPage(std::shared_ptr<QClientModel> model) {
  const auto page = new ClientInfo(std::move(model), this);
  mPages.insertOrAssign(Pages::clientInfo, page);

  const int index = mUi->stackedWidget->addWidget(page);
  mPagesIndex.insertOrAssign(Pages::clientInfo, index);

  mClientPageActions = new QWidget(this);
  auto *actionsLayout = new QHBoxLayout(mClientPageActions);
  actionsLayout->setContentsMargins(0, 0, 0, 0);
  actionsLayout->setSpacing(pcm::widgets::constants::kPanelPadding);

  mClientSearchInput = new QLineEdit(mClientPageActions);
  mClientSearchInput->setPlaceholderText(tr("Search clients"));
  mClientSearchInput->setClearButtonEnabled(true);
  mClientSearchInput->setMinimumWidth(260);

  mAddClientButton = new QPushButton(QIcon(":/icons/user-plus-solid-full.svg"),
                                     tr("Add client"), mClientPageActions);
  mAddClientButton->setIconSize(QSize(16, 16));
  mAddClientButton->setCursor(Qt::PointingHandCursor);

  actionsLayout->addWidget(mClientSearchInput, 1);
  actionsLayout->addWidget(mAddClientButton, 0);
  setPageCustomWidget(Pages::clientInfo, mClientPageActions);
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

  connect(mClientSearchInput, &QLineEdit::textChanged, clientInfoPage,
          &ClientInfo::setSearchQuery);

  connect(mAddClientButton, &QPushButton::clicked, this, [this, clientCardPage]() {
    clientCardPage->setClientInfo(std::nullopt);
    clientCardPage->enterInEditMode();
    showPage(Pages::clientCard, mBtnProfile);
  });

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
    widget->hide();
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

void MainWindow::setupUtilityButtons() {
  const auto makeUtilityButton = [this](const QIcon &icon, const QString &text) {
    auto *button = new QPushButton(icon, text, this);
    button->setFlat(true);
    button->setCheckable(false);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setIconSize(QSize(16, 16));
    button->setStyleSheet(
        "QPushButton {"
        " color: rgba(255, 255, 255, 0.52);"
        " background: transparent;"
        " border: none;"
        " padding: 6px 10px;"
        " text-align: left;"
        "}"
        "QPushButton:hover {"
        " color: rgba(255, 255, 255, 0.78);"
        "}"
        "QPushButton:pressed {"
        " color: rgba(255, 255, 255, 0.92);"
        "}");
    return button;
  };

  mBtnSettings =
      makeUtilityButton(QIcon(":/icons/users-gear-solid-full.svg"), tr("Settings"));
  mBtnAbout = makeUtilityButton(QIcon(":/icons/brain-solid-full.svg"), tr("About"));

  mUi->verticalLayout->addWidget(mBtnSettings);
  mUi->verticalLayout->addWidget(mBtnAbout);

  connect(mBtnSettings, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);
  connect(mBtnAbout, &QPushButton::clicked, this, &MainWindow::openAboutDialog);
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
