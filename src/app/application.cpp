#include "application.h"

Q_LOGGING_CATEGORY(logApplication, "pcm.Application")

namespace pcm {

Application::Application() {
  mDb = std::make_shared<database::Database>(mConf);
}

int Application::run(int argc, char *argv[]) {
  QApplication app(argc, argv);
  mMainWindow = std::make_unique<MainWindow>();

  mMainWindow->addEventInfoPage(mDb);
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
void Application::saveClientEventPair(const obx_id clientId,
                                      const obx_id eventId) const {
  qCDebug(logApplication) << "Application::saveClientEventPair| Client id:"
                          << clientId << " Event id:" << eventId;

  mDb->add_event_client(eventId, clientId);
}

void Application::connectSignals() {
  connect(mMainWindow.get(), &MainWindow::provideSaveClient, this,
          &Application::saveClient);
  connect(mMainWindow.get(), &MainWindow::provideClientEventPairSave, this,
          &Application::saveClientEventPair);
}

} // namespace pcm
