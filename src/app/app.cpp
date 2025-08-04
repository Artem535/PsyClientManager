#include "app.h"
#include "clientmodel.h"

namespace pcm {

Application::Application() {
  mDb = std::make_shared<database::Database>(mConf);
}

int Application::run(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow window;

  window.add_client_info_page(std::make_shared<ClientModel>(mDb));
  window.add_event_info_page(mDb);

  window.show();

  return app.exec();
}

} // namespace pcm
