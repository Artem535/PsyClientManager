#include "backup_encryption_policy.h"

namespace pcm::backup {

bool automaticEncryptedBackupAllowed(const bool encryptionEnabled,
                                     const BackupKeySource keySource) {
  return encryptionEnabled && keySource == BackupKeySource::Keychain;
}

} // namespace pcm::backup
