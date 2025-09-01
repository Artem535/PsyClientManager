#pragma once

#include <QApplication>

#include <memory>

#include "config.h"
#include "database.h"
#include "main_window.h"
#include "qclient_model.h"

namespace pcm {

class Application {
public:
  Application();
  int run(int argc, char *argv[]);

private:
  std::shared_ptr<database::Database> mDb;
  config::Config mConf;
};

} // namespace pcm