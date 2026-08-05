#pragma once

#include <QObject>
#include <QTimer>
#include <memory>
#include <optional>

#include "credential_store.h"
#include "database.h"

namespace pcm::backup {

class AutoBackupScheduler : public QObject {
  Q_OBJECT

public:
  explicit AutoBackupScheduler(std::shared_ptr<database::Database> db,
                               QObject *parent = nullptr,
                               CredentialStore *credentialStore = nullptr);

  void start();
  bool isDue() const;
  void runAsync();

signals:
  void backupFinished(bool ok, const QString &error);

private:
  void startBackupWorker(std::optional<MasterKey> masterKey = std::nullopt);
  void finishBackup(bool ok, const QString &error, const QString &destinationDir);
  void onWorkspaceMasterKeyRead(bool ok, const MasterKey &key,
                                const QString &error);

  std::shared_ptr<database::Database> mDb;
  QTimer mTimer;
  CredentialStore *mCredentialStore = nullptr;
  QString mWorkspaceUuid;
  QString mDestinationDir;
  std::optional<RecoveryEnvelope> mRecoveryEnvelope;
  bool mRunInProgress = false;
};

} // namespace pcm::backup
