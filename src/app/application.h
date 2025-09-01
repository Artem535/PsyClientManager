#pragma once

#include "clientmodel.h"
#include "config.h"
#include "database.h"
#include "mainwindow.h"
#include <QApplication>
#include <memory>

namespace pcm {

class application {
public:
  application();
  int run(int argc, char *argv[]);

private:
  std::shared_ptr<database::Database> mDb;
  config::Config mConf;
};

} // namespace pcm