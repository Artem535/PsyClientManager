#pragma once

#include <QApplication>
#include <QObject>
#include <QLoggingCategory>

#include <memory>

#include "config.h"
#include "database.h"
#include "main_window.h"
#include "qclient_model.h"
#include "event_info.h"

namespace pcm {

class Application final : public QObject {
  Q_OBJECT

public:
  Application();
  int run(int argc, char *argv[]);

private slots:
  void saveClient(const ObxClient &client) const;
  void fillClientComboBox(QComboBox *box) const;
  void saveClientEventPair(const int64_t clientId, const int64_t eventId) const;

private:
  std::unique_ptr<MainWindow> mMainWindow;
  std::shared_ptr<database::Database> mDb;
  config::Config mConf;

  void connectSignals();
};

} // namespace pcm