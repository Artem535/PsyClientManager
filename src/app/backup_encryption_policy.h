#pragma once

namespace pcm::backup {

enum class BackupKeySource { Keychain, RecoveryOnly, Unavailable };

bool automaticEncryptedBackupAllowed(bool encryptionEnabled,
                                     BackupKeySource keySource);

} // namespace pcm::backup
