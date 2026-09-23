#include "client_workspace_page.h"

#include <QVBoxLayout>

ClientWorkspacePage::ClientWorkspacePage(
    std::shared_ptr<QClientModel> clientModel,
    std::shared_ptr<pcm::database::Database> db, QWidget *parent)
    : QWidget(parent) {
  mClientList = new ClientInfo(std::move(clientModel), this);
  mClientCard = new QClientInfoCardPage(db, this);
  mClientNotes = new ClientNotesPage(std::move(db), this);

  mBackButton = new QPushButton(tr("Back to clients"), this);
  mBackButton->setFlat(true);
  mBackButton->setCursor(Qt::PointingHandCursor);

  mDetailTabs = new QTabWidget(this);
  mDetailTabs->addTab(mClientCard, tr("Info"));
  mDetailTabs->addTab(mClientNotes, tr("Notes"));

  auto *detailPage = new QWidget(this);
  auto *detailLayout = new QVBoxLayout(detailPage);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  detailLayout->setSpacing(4);
  detailLayout->addWidget(mBackButton, 0, Qt::AlignLeft);
  detailLayout->addWidget(mDetailTabs, 1);

  mStack = new QStackedWidget(this);
  mStack->insertWidget(kListPage, mClientList);
  mStack->insertWidget(kDetailPage, detailPage);
  mStack->setCurrentIndex(kListPage);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(mStack);

  connect(mBackButton, &QPushButton::clicked, this,
          &ClientWorkspacePage::showList);

  connect(mClientList, &ClientInfo::displayButtonClicked, this,
          [this](const std::optional<DuckClient> &client) {
            selectClient(client, 0);
          });
  connect(mClientList, &ClientInfo::notesButtonClicked, this,
          [this](const std::optional<DuckClient> &client) {
            selectClient(client, 1);
          });
  connect(mClientList, &ClientInfo::removeButtonClicked, this,
          &ClientWorkspacePage::removeButtonClicked);

  connect(mClientNotes, &ClientNotesPage::openClientCardRequested, this,
          [this](const std::optional<DuckClient> &client) {
            mClientCard->setClientInfo(client);
            mDetailTabs->setCurrentIndex(0);
            mStack->setCurrentIndex(kDetailPage);
          });
  connect(mClientNotes, &ClientNotesPage::openEventRequested, this,
          &ClientWorkspacePage::openEventRequested);

  connect(mClientCard, &QClientInfoCardPage::provideSaveClient, this,
          &ClientWorkspacePage::provideSaveClient);
}

void ClientWorkspacePage::selectClient(
    const std::optional<DuckClient> &client, const int tabIndex) {
  mClientCard->setClientInfo(client);
  mClientNotes->setClientInfo(client);
  mDetailTabs->setCurrentIndex(tabIndex);
  mStack->setCurrentIndex(kDetailPage);
}

void ClientWorkspacePage::showList() {
  mStack->setCurrentIndex(kListPage);
}
