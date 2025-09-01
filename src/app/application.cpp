#include "application.h"

namespace pcm {

application::application() {
  mDb = std::make_shared<database::Database>(mConf);
}

int application::run(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow window;

  window.add_event_info_page(mDb);
  window.add_client_info_page(std::make_shared<QClientModel>(mDb));
  window.add_detail_client_info_page();

  window.show();

  return app.exec();
}

} // namespace pcm
