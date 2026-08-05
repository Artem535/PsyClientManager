#include "credential_store.h"

#include <keychain.h>

#include <algorithm>
#include <cstring>

namespace pcm::backup {
namespace {

constexpr auto kKeychainService = "PsyClientManager";

QString keychainError() { return QStringLiteral("system keychain unavailable"); }

} // namespace

QString workspaceBackupKeychainEntry(const QString &workspaceUuid) {
  return QStringLiteral("workspace/%1/backup-master-key").arg(workspaceUuid);
}

QtKeychainCredentialStore::QtKeychainCredentialStore(QObject *parent)
    : CredentialStore(parent) {
  qRegisterMetaType<pcm::backup::MasterKey>();
}

void QtKeychainCredentialStore::readWorkspaceMasterKey(
    const QString &workspaceUuid) {
  auto *job = new QKeychain::ReadPasswordJob(QString::fromLatin1(kKeychainService), this);
  job->setInsecureFallback(false);
  job->setKey(workspaceBackupKeychainEntry(workspaceUuid));
  connect(job, &QKeychain::Job::finished, this,
          [this, job](QKeychain::Job *) {
            if (job->error() != QKeychain::NoError) {
              emit readFinished(false, {}, keychainError());
              job->deleteLater();
              return;
            }

            auto encodedKey = job->textData().toUtf8();
            const auto decodedKey = QByteArray::fromBase64(
                encodedKey, QByteArray::Base64Encoding |
                                QByteArray::AbortOnBase64DecodingErrors);
            std::fill(encodedKey.begin(), encodedKey.end(), '\0');
            if (decodedKey.size() != static_cast<qsizetype>(MasterKey{}.bytes.size())) {
              emit readFinished(false, {}, keychainError());
              job->deleteLater();
              return;
            }

            MasterKey key;
            std::memcpy(key.bytes.data(), decodedKey.constData(), key.bytes.size());
            emit readFinished(true, key, {});
            job->deleteLater();
          });
  job->start();
}

void QtKeychainCredentialStore::writeWorkspaceMasterKey(
    const QString &workspaceUuid, const MasterKey &key) {
  auto *job = new QKeychain::WritePasswordJob(QString::fromLatin1(kKeychainService), this);
  job->setInsecureFallback(false);
  job->setKey(workspaceBackupKeychainEntry(workspaceUuid));
  const auto encodedKey = QByteArray(
      reinterpret_cast<const char *>(key.bytes.data()), static_cast<qsizetype>(key.bytes.size()))
                              .toBase64();
  job->setTextData(QString::fromLatin1(encodedKey));
  connect(job, &QKeychain::Job::finished, this,
          [this, job](QKeychain::Job *) {
            emit writeFinished(job->error() == QKeychain::NoError,
                               job->error() == QKeychain::NoError ? QString{}
                                                                     : keychainError());
            job->deleteLater();
          });
  job->start();
}

} // namespace pcm::backup
