#pragma once

#include "config.h"
#include "database.h"
#include <QApplication>
#include "mainwindow.h"
#include <memory>

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