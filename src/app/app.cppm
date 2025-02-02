module;

#include "mainwindow.h"
#include <slint.h>

export module app;

import widgets.calendar;
import app_database;
import config;

namespace pcm_calc = pcm::widgets::calendar;

export namespace pcm {
class Application {
public:
  Application() {
    pcm_calc::connect_logic(m_window->global<pcm_calc::ICalendarLogic>());
    m_db = std::make_unique<database::Database>(m_conf);
    
  };

  int run() {
    m_window->run();
    return 0;
  }

private:
  slint::ComponentHandle<MainWindow> m_window{MainWindow::create()};
  std::unique_ptr<database::Database> m_db;
  config::Config m_conf; 

};

} // namespace pcm