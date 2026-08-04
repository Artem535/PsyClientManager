#pragma once

#include <array>
#include <string>
#include <string_view>

namespace pcm::backup {

enum class BackupContainerKind { PlainZip, Encrypted, Unknown };

struct MasterKey {
  std::array<unsigned char, 32> bytes{};
};

struct CryptoResult {
  bool ok = false;
  std::string error;
};

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
