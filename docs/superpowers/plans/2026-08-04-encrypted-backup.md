# Encrypted Backup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create and restore passphrase-recoverable encrypted `.psybackup` files while preserving existing plain-ZIP backups.

**Architecture:** `src/backup` gets a pure C++ libsodium container module that encrypts and decrypts a complete ZIP stream. QtKeychain remains in an app-level `CredentialStore` adapter: it reads the workspace master key before a background backup/restore worker starts, so cryptographic services have no Qt or platform-keychain dependency.

**Tech Stack:** C++20, libsodium, Qt6 Widgets, QtKeychain, Poco, DuckDB, GoogleTest.

## Global Constraints

- Implement ADR `docs/asciidoc/10-encrypted-backup-adr.adoc` exactly; do not add ad-hoc crypto.
- New encrypted files start with `PCMENC01`; plain ZIP backups remain valid and restorable.
- Use Argon2id moderate parameters recorded in the encrypted header and `crypto_secretstream_xchacha20poly1305` for the archive body.
- Do not store passwords, derived keys, or master keys in QSettings, logs, manifests, or errors.
- An unavailable keychain disables automatic encrypted backup; it must never cause an unencrypted fallback.
- A manual backup without keychain receives a new random recovery-only key and
  must never update the workspace key or enable automatic backup.
- Rebase the implementation branch on `main` after PR #65 merges, then set the next available application version and changelog entry.

---

## File Structure

- Create `src/backup/encrypted_container.h/.cpp`: binary container detection, header parsing, password wrapping, and streamed encryption/decryption.
- Create `src/app/credential_store.h/.cpp`: QtKeychain adapter for a workspace master key; no password persistence.
- Modify `src/backup/backup_service.*`, `backup_validator.*`, and `restore_service.*`: route encrypted files through the container while retaining ZIP behavior.
- Create `src/app/backup_encryption_policy.h/.cpp`; modify `src/app/auto_backup_scheduler.*`, `settings_dialog.*`, `application.cpp`, and `src/widgets/app_settings.*`: global encryption preference, password setup, key retrieval, and restore prompt.
- Modify `vcpkg.json`, `src/backup/CMakeLists.txt`, `src/app/CMakeLists.txt`, `test/CMakeLists.txt`, and `test/backup_tests.cpp` for dependencies and coverage.

### Task 1: Add dependencies and a testable encrypted container

**Files:**
- Modify: `vcpkg.json`, `src/backup/CMakeLists.txt`, `test/CMakeLists.txt`
- Create: `src/backup/encrypted_container.h`, `src/backup/encrypted_container.cpp`
- Modify: `test/backup_tests.cpp`

**Interfaces:**

```cpp
namespace pcm::backup {
enum class BackupContainerKind { PlainZip, Encrypted, Unknown };
struct MasterKey { std::array<unsigned char, 32> bytes{}; };
struct CryptoResult { bool ok = false; std::string error; };
BackupContainerKind detect_backup_container(const std::string &path);
CryptoResult encrypt_backup_file(const std::string &zip_path,
                                  const std::string &output_path,
                                  std::string_view recovery_password,
                                  const MasterKey &master_key);
CryptoResult decrypt_backup_file(const std::string &input_path,
                                  const std::string &zip_path,
                                  std::string_view recovery_password,
                                  MasterKey *key_from_password = nullptr);
} // namespace pcm::backup
```

- [ ] **Step 1: Add failing container tests**

```cpp
TEST(EncryptedContainerTest, RoundTripsZipWithRecoveryPassword) {
  const auto plain = writeTempFile("tmp_plain.zip", "PK\x03\x04payload");
  const auto encrypted = tempPath("tmp_encrypted.psybackup");
  const auto restored = tempPath("tmp_restored.zip");
  const auto key = fixedMasterKey();
  ASSERT_TRUE(encrypt_backup_file(plain, encrypted, "correct horse battery staple", key).ok);
  EXPECT_EQ(detect_backup_container(encrypted), BackupContainerKind::Encrypted);
  ASSERT_TRUE(decrypt_backup_file(encrypted, restored,
                                  "correct horse battery staple").ok);
  EXPECT_EQ(readFile(restored), readFile(plain));
}
```

- [ ] **Step 2: Run the new test and verify it fails**

Run: `ctest --test-dir build -R EncryptedContainerTest --output-on-failure`

Expected: compilation failure because `encrypted_container.h` does not exist.

- [ ] **Step 3: Add the libsodium dependency**

Add `libsodium` to `vcpkg.json`. Link the backup target to
`unofficial-sodium::sodium`. Add `qtkeychain-qt6` only in Task 3, where the
app-level credential-store adapter is introduced. Call
`sodium_init()` once in the container implementation and fail closed if it
returns an error.

- [ ] **Step 4: Implement the versioned container**

Write `PCMENC01`, a bounded JSON header, then `secretstream` records. Generate
salt, wrap nonce, master key, and stream header with `randombytes_buf`.
Derive the wrapping key using the recorded Argon2id parameters. Authenticate
the serialized header as additional data for every stream record. Reject an
oversized header, unsupported version, truncated final record, authentication
failure, and any password shorter than 12 characters when creating a backup.

- [ ] **Step 5: Add rejection tests**

```cpp
TEST(EncryptedContainerTest, RejectsWrongPasswordWithoutWritingPlaintext);
TEST(EncryptedContainerTest, RejectsCiphertextTampering);
TEST(EncryptedContainerTest, KeepsPlainZipDetectionCompatible);
```

- [ ] **Step 6: Run focused tests and commit**

Run: `ctest --test-dir build -R 'EncryptedContainerTest|BackupValidatorTest' --output-on-failure`

```bash
git add vcpkg.json src/backup test/CMakeLists.txt test/backup_tests.cpp
git commit -m "feat: add encrypted backup container"
```

### Task 2: Integrate encrypted files with backup validation and restore

**Files:**
- Modify: `src/backup/backup_service.h`, `src/backup/backup_service.cpp`
- Modify: `src/backup/backup_validator.h`, `src/backup/backup_validator.cpp`
- Modify: `src/backup/restore_service.h`, `src/backup/restore_service.cpp`
- Modify: `test/backup_tests.cpp`

**Interfaces:**

```cpp
struct BackupEncryptionOptions {
  std::optional<MasterKey> master_key;
  std::optional<std::string> recovery_password;
};
struct BackupOptions {
  std::optional<std::string> attachments_root;
  std::optional<BackupEncryptionOptions> encryption;
};
struct RestoreOptions {
  std::optional<std::string> attachments_root;
  std::optional<std::string> recovery_password;
};
```

- [ ] **Step 1: Add failing service-level round-trip tests**

```cpp
TEST(BackupServiceTest, EncryptedBackupHidesZipAndValidatesAfterDecryption);
TEST(RestoreServiceTest, RestoresEncryptedBackupWithCorrectPassword);
TEST(RestoreServiceTest, WrongPasswordLeavesExistingDatabaseUntouched);
TEST(RestoreServiceTest, RestoresExistingUnencryptedBackupUnchanged);
```

- [ ] **Step 2: Run the four tests and verify failure**

Run: `ctest --test-dir build -R 'EncryptedBackup|RestoresEncrypted|WrongPassword|UnencryptedBackup' --output-on-failure`

Expected: encrypted `BackupOptions` and password-aware restore path are absent.

- [ ] **Step 3: Make BackupService encrypt only its temporary ZIP**

Keep the existing snapshot, manifest, and ZIP creation logic. When encryption
options are present, create the ZIP under the restricted scratch directory,
stream it to `<destination>.partial-<uuid>`, remove the ZIP through RAII, and
atomically rename only the ciphertext. Preserve the current unencrypted path
when options are absent.

- [ ] **Step 4: Make validator and restore normalize to a temporary ZIP**

Add a single internal helper that detects the container. For ZIP it returns the
input path. For `PCMENC01` it decrypts into a restricted scratch file, then
calls the existing ZIP validator or staged restore logic. Decryption errors
must be returned as `cannot decrypt backup`; never leak whether the password
or ciphertext was wrong. `RestoreService` must normalize before any target
database or attachment path is created or renamed.

- [ ] **Step 5: Run focused tests and commit**

Run: `ctest --test-dir build -R 'BackupServiceTest|BackupValidatorTest|RestoreServiceTest' --output-on-failure`

```bash
git add src/backup test/backup_tests.cpp
git commit -m "feat: restore encrypted backups safely"
```

### Task 3: Add QtKeychain-backed workspace key management

**Files:**
- Create: `src/app/credential_store.h`, `src/app/credential_store.cpp`
- Create: `src/app/backup_encryption_policy.h`, `src/app/backup_encryption_policy.cpp`
- Modify: `src/app/CMakeLists.txt`, `src/widgets/app_settings.h`, `src/widgets/app_settings.cpp`, `test/CMakeLists.txt`
- Modify: `src/app/auto_backup_scheduler.h`, `src/app/auto_backup_scheduler.cpp`
- Test: `test/backup_tests.cpp` with an in-memory fake key provider

**Interfaces:**

```cpp
class CredentialStore : public QObject {
  Q_OBJECT
public:
  virtual void readWorkspaceMasterKey(const QString &workspaceUuid) = 0;
  virtual void writeWorkspaceMasterKey(const QString &workspaceUuid,
                                       const pcm::backup::MasterKey &key) = 0;
signals:
  void readFinished(bool ok, pcm::backup::MasterKey key, QString error);
  void writeFinished(bool ok, QString error);
};

enum class BackupKeySource { Keychain, RecoveryOnly, Unavailable };
bool automaticEncryptedBackupAllowed(bool encryptionEnabled,
                                     BackupKeySource keySource);
```

- [ ] **Step 1: Add failing scheduler tests for policy**

```cpp
TEST(AutoBackupDueTest, EncryptedAutoBackupIsSkippedWhenKeyUnavailable);
TEST(AutoBackupDueTest, EncryptedAutoBackupUsesKeyWhenAvailable);
TEST(AutoBackupDueTest, RecoveryOnlyKeyNeverEnablesAutomaticBackup);
```

- [ ] **Step 2: Run and verify failure**

Run: `ctest --test-dir build -R AutoBackupDueTest --output-on-failure`

Expected: no encrypted-backup policy exists.

- [ ] **Step 3: Implement the adapter and settings**

Implement `QtKeychainCredentialStore` with `QKeychain::ReadPasswordJob` and
`WritePasswordJob`. Store only the 32-byte master key encoded as base64 under
service `PsyClientManager` and key `workspace/<uuid>/backup-master-key`.
Add QSettings values only for `backup/encryptionEnabled` and the keychain entry
identifier. Never place the password or key bytes in QSettings. Extend the
scheduler so it reads the key asynchronously before creating `BackupWorker`;
on failure it emits a failed/skipped result and does not call BackupService.
When a manual operation has no keychain access, generate a fresh master key,
wrap it with the entered recovery password, and set `BackupKeySource::RecoveryOnly`.
`automaticEncryptedBackupAllowed()` returns `true` only for
`BackupKeySource::Keychain`.

- [ ] **Step 4: Run policy tests and commit**

Run: `ctest --test-dir build -R AutoBackupDueTest --output-on-failure`

```bash
git add src/app src/widgets test/backup_tests.cpp
git commit -m "feat: store backup keys in system keychain"
```

### Task 4: Add the encrypted-backup settings and recovery UI

**Files:**
- Modify: `src/app/settings_dialog.h`, `src/app/settings_dialog.cpp`
- Modify: `src/app/application.cpp`, `src/backup/restore_service.h`
- Modify: `translation/app_en.ts`, `translation/app_ru.ts`

- [ ] **Step 1: Add the disabled-by-default encryption setting**

Place it in the existing Backup settings group: a Qlementine switch labelled
`Encrypt backups`, recovery-password and confirmation fields shown only while
enabling, plus a visible warning that the password cannot be recovered. Enable
the setting only after QtKeychain successfully persists a newly generated
master key. Password fields use `QLineEdit::Password` and are cleared after
the operation.

- [ ] **Step 2: Pass keys and passwords through existing worker boundaries**

Manual backup reads the key from keychain before `BackupWorker` starts. If it
is unavailable, prompt for the recovery password and create a recovery-only
backup using a new random master key; do not permit automatic backup in that
state. Restore detects an encrypted file before staging `pending-restore.json`,
asks for the recovery password only when the local key is unavailable, and
writes no password to the marker. On the next launch, Application obtains the
keychain key or asks for the recovery password before calling RestoreService.

- [ ] **Step 3: Synchronize translations**

Run: `cmake --build build-release --target update_translations`

Translate every new source string in both `translation/app_en.ts` and
`translation/app_ru.ts`; verify `rg 'type="unfinished"' translation` returns
no result.

- [ ] **Step 4: Manually verify desktop flows**

1. Enable encryption, enter a 12+ character recovery password, and confirm a
   keychain entry is created without the password in QSettings.
2. Create and validate an encrypted manual backup; confirm it does not open as
   ZIP and can be restored after restart.
3. Restore it with a wrong password; confirm the previous database remains.
4. Disable keychain access; confirm automatic backup reports failure and no
   plain `.psybackup` is created.

- [ ] **Step 5: Commit UI and translations**

```bash
git add src/app src/backup src/widgets translation
git commit -m "feat: add encrypted backup settings"
```

### Task 5: Release verification and documentation

**Files:**
- Modify: `README.md`, `docs/asciidoc/05-modules.adoc`, `CHANGELOG.md`,
  `CMakeLists.txt`, `src/app/application.cpp`

- [ ] **Step 1: Document final behavior**

Describe encrypted backup, recovery-password loss, keychain requirement for
automatic backup, and plain-backup compatibility. Do not document key values,
passwords, or implementation-only temporary paths.

- [ ] **Step 2: Run full verification**

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --build build-release --target update_translations
git diff --check
```

- [ ] **Step 3: Commit release metadata**

```bash
git add README.md docs/asciidoc CHANGELOG.md CMakeLists.txt src/app/application.cpp
git commit -m "docs: describe encrypted backups"
```

## Spec Coverage Review

- Whole-archive encryption and versioned format: Tasks 1 and 2.
- Argon2id moderate, secretstream authentication, and tamper handling: Task 1.
- QtKeychain, one workspace key, and unavailable-keychain policy: Task 3.
- Global opt-in, recovery password, manual/automatic paths, and restore UI: Task 4.
- Wrong password rollback, plaintext compatibility, tests, version, docs, and
  translations: Tasks 2 and 5.
