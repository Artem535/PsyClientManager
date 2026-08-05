#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pcm::backup {

enum class BackupContainerKind { PlainZip, Encrypted, Unknown };

struct MasterKey {
  std::array<unsigned char, 32> bytes{};
};

// This data is safe to persist in application settings. It lets a recovery
// password unwrap the workspace master key but contains neither secret.
struct RecoveryEnvelope {
  std::uint32_t envelope_version = 1;
  std::uint32_t kdf_algorithm = 0;
  std::uint64_t kdf_opslimit = 0;
  std::uint64_t kdf_memlimit = 0;
  std::string salt;
  std::string wrap_nonce;
  std::string wrapped_master_key;
};

struct CryptoResult {
  bool ok = false;
  std::string error;
};

BackupContainerKind detect_backup_container(const std::string &path);

CryptoResult create_recovery_envelope(std::string_view recovery_password,
                                      const MasterKey &master_key,
                                      RecoveryEnvelope *envelope);
CryptoResult validate_recovery_envelope(const RecoveryEnvelope &envelope);
std::string serialize_recovery_envelope(const RecoveryEnvelope &envelope);
std::optional<RecoveryEnvelope>
deserialize_recovery_envelope(std::string_view serialized_envelope);

CryptoResult encrypt_backup_file(const std::string &zip_path,
                                 const std::string &output_path,
                                 std::string_view recovery_password,
                                 const MasterKey &master_key);

CryptoResult encrypt_backup_file(const std::string &zip_path,
                                 const std::string &output_path,
                                 const RecoveryEnvelope &recovery_envelope,
                                 const MasterKey &master_key);

CryptoResult decrypt_backup_file(const std::string &input_path,
                                 const std::string &zip_path,
                                 std::string_view recovery_password,
                                 MasterKey *key_from_password = nullptr);

} // namespace pcm::backup
