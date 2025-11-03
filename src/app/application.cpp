#include "application.h"

Q_LOGGING_CATEGORY(logApplication, "pcm.Application")

namespace pcm {

Application::Application() {
  mDb = std::make_shared<database::Database>(mConf);
}

int Application::run(int argc, char *argv[]) {
  QApplication app(argc, argv);
  mMainWindow = std::make_unique<MainWindow>();

  mMainWindow->addEventInfoPage(new QTimelineModel(mDb, this));
  mMainWindow->addClientInfoPage(std::make_shared<QClientModel>(mDb));
  mMainWindow->addClientCardPage();
  mMainWindow->connectSignals();
  connectSignals();

  mMainWindow->show();

  return app.exec();
}
void Application::saveClient(const ObxClient &client) const {
  qCDebug(logApplication) << "Application::saveClient| Client id:" << client.id;
  mDb->add_client(client);
}

void Application::fillClientComboBox(QComboBox *box) const {
  box->clear();
  const auto clients = mDb->get_clients();
  for (const auto &client : clients) {
    QString title{"%1 %2"};
    const auto name = QString::fromStdString(client->name.value_or("Undefined"));
    const auto lastname = QString::fromStdString(client->last_name.value_or("Undefined"));
    title = title.arg(name, lastname);
    const auto var = QVariant::fromValue(client->id);
    box->addItem(title, var);
  }
}

void Application::saveClientEventPair(const int64_t clientId,
                                      const int64_t eventId) const {
  qCDebug(logApplication) << "Application::saveClientEventPair| Client id:"
                          << clientId << " Event id:" << eventId;

  mDb->add_event_client(eventId, clientId);
}

void Application::connectSignals() {
  connect(mMainWindow.get(), &MainWindow::provideSaveClient, this,
          &Application::saveClient);
  connect(mMainWindow.get(), &MainWindow::provideClientEventPairSave, this,
          &Application::saveClientEventPair);

  {
    const auto widget = mMainWindow->getPage(MainWindow::Pages::eventInfo);
    const auto page = dynamic_cast<QEventInfoPage *>(widget);
    connect(page, &QEventInfoPage::provideFillClientComboBox, this,
            &Application::fillClientComboBox);
    connect(page, &QEventInfoPage::provideClientByEventId, [page, this](int64_t eventId) {
      const auto client = mDb->get_client_by_event(eventId);
      emit page->clientResolved(client.id);
    });

  }
}

} // namespace pcm
