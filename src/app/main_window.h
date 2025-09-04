#pragma once

#include "client_info.h"
#include "event_info.h"
#include "database.h"
#include "client_info_card.h"
#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  enum class Pages {clientInfo, eventInfo, clientCard};

  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  void addClientInfoPage(std::shared_ptr<QClientModel> model);
  void addEventInfoPage(std::shared_ptr<pcm::database::Database> db);
  void addClientCardPage();
  void connectSignals();

signals:
  void provideSaveClient(const ObxClient &client);

private:
  QHash<Pages, QWidget*> mPages;
  QHash<Pages, int> mPagesIndex;
  std::unique_ptr<Ui::MainWindow> mUi;
  std::shared_ptr<pcm::database::Database> mDb{nullptr};
};
