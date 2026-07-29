#include "auto_backup_scheduler.h"

#include "../backup/auto_backup_due.h"
#include "../backup/backup_rotation_service.h"
#include "../backup/backup_service.h"
#include "../widgets/app_settings.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QThread>

namespace pcm::backup {
namespace {

constexpr int kAutoBackupTimerIntervalMs = 60 * 60 * 1000;

class AutoBackupWorker final : public QObject {
  Q_OBJECT

public:
  AutoBackupWorker(std::shared_ptr<database::Database> db, QString destinationPath,
                   QString attachmentsRoot)
      : mDb(std::move(db)), mDestinationPath(std::move(destinationPath)),
        mAttachmentsRoot(std::move(attachmentsRoot)) {}

public slots:
  void run() {
    BackupService service;
    BackupOptions options;
    options.attachments_root = mAttachmentsRoot.toStdString();
    const auto result =
        service.create_backup(*mDb, mDestinationPath.toStdString(), options);
    emit finished(result.ok, QString::fromStdString(result.error));
  }

signals:
  void finished(bool ok, const QString &error);

private:
  std::shared_ptr<database::Database> mDb;
  QString mDestinationPath;
  QString mAttachmentsRoot;
};

} // namespace

AutoBackupScheduler::AutoBackupScheduler(std::shared_ptr<database::Database> db,
                                         QObject *parent)
    : QObject(parent), mDb(std::move(db)) {}

void AutoBackupScheduler::start() {
  mTimer.setInterval(kAutoBackupTimerIntervalMs);
  connect(&mTimer, &QTimer::timeout, this, &AutoBackupScheduler::runAsync);
  mTimer.start();
  runAsync();
}

bool AutoBackupScheduler::isDue() const {
  return isAutoBackupDue(app_settings::autoBackupEnabled(),
                         app_settings::autoBackupLastRunAtMs(),
                         app_settings::autoBackupIntervalDays(),
                         QDateTime::currentMSecsSinceEpoch());
}

void AutoBackupScheduler::runAsync() {
  if (mRunInProgress || !isDue()) {
    return;
  }
  mRunInProgress = true;

  const auto destinationDir = app_settings::autoBackupDestination();
  QDir().mkpath(destinationDir);
  const auto destinationPath =
      QDir(destinationDir)
          .filePath(QStringLiteral("PsyClientManager-auto-%1.psybackup")
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));

  auto *thread = new QThread(this);
  auto *worker = new AutoBackupWorker(mDb, destinationPath,
                                      app_settings::attachmentsStorageRoot());
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &AutoBackupWorker::run);
  connect(worker, &AutoBackupWorker::finished, this,
          [this, destinationDir](const bool ok, const QString &error) {
            mRunInProgress = false;
            if (ok) {
              app_settings::setAutoBackupLastRunAtMs(
                  QDateTime::currentMSecsSinceEpoch());
              BackupRotationService rotation;
              const auto rotationResult =
                  rotation.prune(destinationDir.toStdString(),
                                "PsyClientManager-auto-",
                                app_settings::autoBackupKeepCount());
              if (!rotationResult.ok) {
                qWarning() << "AutoBackupScheduler: rotation failed:"
                           << QString::fromStdString(rotationResult.error);
              }
            } else {
              qWarning() << "AutoBackupScheduler: automatic backup failed:" << error;
            }
            emit backupFinished(ok, error);
          });
  connect(worker, &AutoBackupWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}

} // namespace pcm::backup

#include "auto_backup_scheduler.moc"
