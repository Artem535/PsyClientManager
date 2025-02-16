#include "app.h"


namespace pcm {

Application::Application() {
    m_db = std::make_shared<database::Database>(m_conf);
}

int Application::run(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window(m_db);
    window.show();
    return app.exec();
}

} // namespace pcm
