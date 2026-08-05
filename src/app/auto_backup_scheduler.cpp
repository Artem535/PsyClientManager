#include "auto_backup_scheduler.h"

#include "../backup/auto_backup_due.h"
#include "../backup/backup_rotation_service.h"
#include "../backup/backup_service.h"
#include "../widgets/app_settings.h"
#include "backup_encryption_policy.h"

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
                   QString attachmentsRoot, std::optional<MasterKey> masterKey,
                   std::optional<RecoveryEnvelope> recoveryEnvelope)
      : mDb(std::move(db)), mDestinationPath(std::move(destinationPath)),
        mAttachmentsRoot(std::move(attachmentsRoot)), mMasterKey(std::move(masterKey)),
        mRecoveryEnvelope(std::move(recoveryEnvelope)) {}

public slots:
  void run() {
    BackupService service;
    BackupOptions options;
    options.attachments_root = mAttachmentsRoot.toStdString();
    if (mMasterKey.has_value()) {
      options.encryption = BackupEncryptionOptions{
          .master_key = mMasterKey,
          .recovery_envelope = mRecoveryEnvelope,
      };
    }
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
  std::optional<MasterKey> mMasterKey;
  std::optional<RecoveryEnvelope> mRecoveryEnvelope;
};

} // namespace

AutoBackupScheduler::AutoBackupScheduler(std::shared_ptr<database::Database> db,
                                         QObject *parent,
                                         CredentialStore *credentialStore)
    : QObject(parent), mDb(std::move(db)), mCredentialStore(credentialStore) {
  if (mCredentialStore == nullptr) {
    mCredentialStore = new QtKeychainCredentialStore(this);
  }
  connect(mCredentialStore, &CredentialStore::readFinished, this,
          &AutoBackupScheduler::onWorkspaceMasterKeyRead);
}

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
  mDestinationDir = app_settings::autoBackupDestination();

  if (app_settings::backupEncryptionEnabled()) {
    mRecoveryEnvelope = deserialize_recovery_envelope(
        app_settings::backupEncryptionRecoveryEnvelope().toStdString());
    if (!mRecoveryEnvelope.has_value()) {
      finishBackup(false,
                   QStringLiteral("encrypted automatic backup recovery envelope is unavailable"),
                   mDestinationDir);
      return;
    }
    const auto metadata = mDb->get_application_metadata();
    mWorkspaceUuid = QString::fromStdString(metadata.workspace_uuid);
    const auto expectedEntry = workspaceBackupKeychainEntry(mWorkspaceUuid);
    if (mWorkspaceUuid.isEmpty() ||
        app_settings::backupEncryptionKeychainEntry() != expectedEntry) {
      finishBackup(false, QStringLiteral("encrypted automatic backup key is unavailable"),
                   mDestinationDir);
      return;
    }
    mCredentialStore->readWorkspaceMasterKey(mWorkspaceUuid);
    return;
  }

  startBackupWorker();
}

void AutoBackupScheduler::onWorkspaceMasterKeyRead(const bool ok,
                                                    const MasterKey &key,
                                                    const QString &error) {
  if (!mRunInProgress) {
    return;
  }
  const auto keySource = ok ? BackupKeySource::Keychain
                            : BackupKeySource::Unavailable;
  if (!automaticEncryptedBackupAllowed(app_settings::backupEncryptionEnabled(),
                                       keySource)) {
    finishBackup(false, error.isEmpty()
                            ? QStringLiteral("encrypted automatic backup key is unavailable")
                            : error,
                 mDestinationDir);
    return;
  }
  startBackupWorker(key);
}

void AutoBackupScheduler::startBackupWorker(std::optional<MasterKey> masterKey) {
  const auto destinationDir = mDestinationDir;
  QDir().mkpath(destinationDir);
  const auto destinationPath =
      QDir(destinationDir)
          .filePath(QStringLiteral("PsyClientManager-auto-%1.psybackup")
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));

  auto *thread = new QThread(this);
  auto *worker = new AutoBackupWorker(mDb, destinationPath,
                                      app_settings::attachmentsStorageRoot(),
                                      std::move(masterKey), mRecoveryEnvelope);
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &AutoBackupWorker::run);
  connect(worker, &AutoBackupWorker::finished, this,
          [this, destinationDir](const bool ok, const QString &error) {
            finishBackup(ok, error, destinationDir);
          });
  connect(worker, &AutoBackupWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}

void AutoBackupScheduler::finishBackup(const bool ok, const QString &error,
                                       const QString &destinationDir) {
  mRunInProgress = false;
  if (ok) {
    app_settings::setAutoBackupLastRunAtMs(QDateTime::currentMSecsSinceEpoch());
    BackupRotationService rotation;
    const auto rotationResult = rotation.prune(destinationDir.toStdString(),
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
}

} // namespace pcm::backup

#include "auto_backup_scheduler.moc"
