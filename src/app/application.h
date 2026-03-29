#pragma once

#include <QApplication>
#include <QEvent>
#include <QObject>
#include <QLoggingCategory>
#include <QSystemTrayIcon>
#include <QTimer>

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
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void saveClient(const DuckClient &client);
  void removeClient(int64_t clientId);
  void fillClientComboBox(QComboBox *box);
  void saveClientEventPair(const int64_t clientId, const int64_t eventId);
  void checkUpcomingEventNotifications();
  void restoreMainWindow();
  void quitApplication();

private:
  QString notificationKey(const DuckEvent &event) const;
  QString notificationTitleForEvent(const DuckEvent &event) const;
  QString notificationBodyForEvent(const DuckEvent &event) const;
  void initializeNotifications();

  std::unique_ptr<MainWindow> mMainWindow;
  std::shared_ptr<database::Database> mDb;
  std::shared_ptr<QClientModel> mClientModel;
  std::unique_ptr<QSystemTrayIcon> mTrayIcon;
  QTimer mNotificationTimer;
  bool mIsQuitting = false;
  bool mTrayCloseHintShown = false;
  config::Config mConf;

  void connectSignals();
};

} // namespace pcm
