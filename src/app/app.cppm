module;

#include "mainwindow.h"
#include <slint.h>

export module app;

import widgets.calendar;

namespace pcm_calc = pcm::widgets::calendar;

export namespace pcm {
class Application {
public:
  Application() {
    pcm_calc::connect_logic(m_window->global<pcm_calc::ICalendarLogic>());
  };

  int run() {
    m_window->run();
    return 0;
  }

private:
  slint::ComponentHandle<MainWindow> m_window{MainWindow::create()};
};

} // namespace pcm