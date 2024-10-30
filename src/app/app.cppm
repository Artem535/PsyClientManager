module;

#include "mainwindow.h"
#include <slint.h>

export module app;

export namespace pcm {

class Application {
public:
  Application() = default;

  int run() {
    m_window->run();
    return 0;
  }

private:
  slint::ComponentHandle<MainWindow> m_window{MainWindow::create()};
};

} // namespace pcm