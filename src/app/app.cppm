module;

#include "mainwindow.h"
#include <memory>
#include <slint.h>

export module app;

import widgets.calendar;
import app_database;
import slint_database_adapter;
import config;

namespace pcm_calc = pcm::widgets::calendar;
namespace pcm_db_adapter = pcm::database::adapters;

export namespace pcm {
class Application {
public:
  Application() {
    m_db = std::make_shared<database::Database>(m_conf);
    m_adapter = std::make_unique<database::adapters::SlintDbAdapter>(m_db);
  };

  int run() {
    // pcm_calc::connect_logic(m_window->global<pcm_calc::ICalendarLogic>());
    m_adapter->connect_logic(
        m_window->global<pcm_db_adapter::IClientInfoLogic>());
    m_window->run();
    return 0;
  }

private:
  slint::ComponentHandle<MainWindow> m_window{MainWindow::create()};
  std::shared_ptr<database::Database> m_db;
  std::unique_ptr<database::adapters::SlintDbAdapter> m_adapter;
  config::Config m_conf;
};

} // namespace pcm