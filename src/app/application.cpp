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

void Application::connectSignals() {
  connect(mMainWindow.get(), &MainWindow::provideSaveClient, this,
          &Application::saveClient);
}

} // namespace pcm
