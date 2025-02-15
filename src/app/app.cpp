#include "app.h"

namespace pcm {

Application::Application() {
    m_db = std::make_shared<database::Database>(m_conf);
}

int Application::run() {
    return 0;
}

} // namespace pcm
