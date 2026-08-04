#pragma once

#include "encrypted_container.h"

#include <QObject>
#include <QString>

Q_DECLARE_METATYPE(pcm::backup::MasterKey)

namespace pcm::backup {

QString workspaceBackupKeychainEntry(const QString &workspaceUuid);

class CredentialStore : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~CredentialStore() override = default;

  virtual void readWorkspaceMasterKey(const QString &workspaceUuid) = 0;
  virtual void writeWorkspaceMasterKey(const QString &workspaceUuid,
                                       const MasterKey &key) = 0;

signals:
  void readFinished(bool ok, pcm::backup::MasterKey key, const QString &error);
  void writeFinished(bool ok, const QString &error);
};

class QtKeychainCredentialStore final : public CredentialStore {
  Q_OBJECT

public:
  explicit QtKeychainCredentialStore(QObject *parent = nullptr);

  void readWorkspaceMasterKey(const QString &workspaceUuid) override;
  void writeWorkspaceMasterKey(const QString &workspaceUuid,
                               const MasterKey &key) override;
};

} // namespace pcm::backup
