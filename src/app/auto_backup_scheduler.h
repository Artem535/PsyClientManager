#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

#include "database.h"

namespace pcm::backup {

class AutoBackupScheduler : public QObject {
  Q_OBJECT

public:
  explicit AutoBackupScheduler(std::shared_ptr<database::Database> db,
                               QObject *parent = nullptr);

  void start();
  bool isDue() const;
  void runAsync();

signals:
  void backupFinished(bool ok, const QString &error);

private:
  std::shared_ptr<database::Database> mDb;
  QTimer mTimer;
  bool mRunInProgress = false;
};

} // namespace pcm::backup
