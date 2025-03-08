module;

#include "mainwindow.h"
#include <iostream>
#include <memory>
#include <slint.h>

export module app;

import widgets.calendar;
import app_database;
import slint_database_adapter;
import config;
import logic;

namespace pcm_calc = pcm::widgets::calendar;
namespace pcm_db_adapter = pcm::database::adapters;

export namespace pcm {
class Application {
public:
  Application() {
    m_db = std::make_shared<database::Database>(m_conf);
    m_adapter = std::make_unique<pcm_db_adapter::SlintDbAdapter>(m_db);
  };

  int run() {
    pcm::logic::connect_db(m_window->global<pcm::logic::IClientInfoLogic>(), *m_adapter);
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