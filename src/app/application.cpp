#include "application.h"

namespace pcm {

Application::Application() {
  mDb = std::make_shared<database::Database>(mConf);
}

int Application::run(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow window;

  window.addEventInfoPage(mDb);
  window.addClientInfoPage(std::make_shared<QClientModel>(mDb));
  window.addClientCardPage();

  window.show();

  return app.exec();
}

} // namespace pcm
