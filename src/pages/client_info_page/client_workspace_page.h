#pragma once

#include "client_info.h"
#include "client_info_card.h"
#include "client_notes_page.h"

#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QWidget>

#include <memory>

class ClientWorkspacePage final : public QWidget {
  Q_OBJECT

public:
  explicit ClientWorkspacePage(std::shared_ptr<QClientModel> clientModel,
                               std::shared_ptr<pcm::database::Database> db,
                               QWidget *parent = nullptr);

  [[nodiscard]] ClientInfo *clientList() const { return mClientList; }
  [[nodiscard]] QClientInfoCardPage *clientCard() const { return mClientCard; }
  [[nodiscard]] ClientNotesPage *clientNotes() const { return mClientNotes; }

signals:
  void removeButtonClicked(int64_t clientId);
  void provideSaveClient(const DuckClient &client);
  void openEventRequested(int64_t eventId, qint64 dayMs);

public slots:
  void selectClient(const std::optional<DuckClient> &client, int tabIndex);
  void showList();

private:
  QStackedWidget *mStack{nullptr};
  QTabWidget *mDetailTabs{nullptr};
  QPushButton *mBackButton{nullptr};
  ClientInfo *mClientList{nullptr};
  QClientInfoCardPage *mClientCard{nullptr};
  ClientNotesPage *mClientNotes{nullptr};

  enum StackIndex { kListPage = 0, kDetailPage = 1 };
};
