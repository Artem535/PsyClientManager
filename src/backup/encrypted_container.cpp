#include "encrypted_container.h"

#include <Poco/Exception.h>
#include <Poco/File.h>
#include <sodium.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <rfl/json.hpp>
#include <string>
#include <system_error>
#include <vector>

namespace pcm::backup {
namespace {

constexpr std::string_view kMagic = "PCMENC01";
constexpr std::uint32_t kLegacyContainerVersion = 1;
constexpr std::uint32_t kCurrentContainerVersion = 2;
constexpr std::uint32_t kRecoveryEnvelopeVersion = 1;
constexpr std::uint32_t kMaxHeaderSize = 16 * 1024;
constexpr std::uint32_t kChunkSize = 64 * 1024;
constexpr std::uint64_t kMaxKdfMemory = crypto_pwhash_MEMLIMIT_MODERATE;
constexpr std::uint64_t kMaxKdfOperations = crypto_pwhash_OPSLIMIT_MODERATE;

// The v1 layout and its JSON field order are part of the encrypted backup
// format. Keep it separate from v2: it is serialized as authenticated data.
struct LegacyHeaderForWrap {
  std::uint32_t container_version = kLegacyContainerVersion;
  std::uint32_t kdf_algorithm = crypto_pwhash_ALG_ARGON2ID13;
  std::uint64_t kdf_opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
  std::uint64_t kdf_memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
  std::string salt;
  std::string wrap_nonce;
  std::string stream_header;
  std::uint32_t chunk_size = kChunkSize;
};

struct LegacyContainerHeader {
  std::uint32_t container_version = kLegacyContainerVersion;
  std::uint32_t kdf_algorithm = crypto_pwhash_ALG_ARGON2ID13;
  std::uint64_t kdf_opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
  std::uint64_t kdf_memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
  std::string salt;
  std::string wrap_nonce;
  std::string stream_header;
  std::uint32_t chunk_size = kChunkSize;
  std::string wrapped_master_key;
};

struct ContainerHeader {
  std::uint32_t container_version = kCurrentContainerVersion;
  std::uint32_t envelope_version = kRecoveryEnvelopeVersion;
  std::uint32_t kdf_algorithm = crypto_pwhash_ALG_ARGON2ID13;
  std::uint64_t kdf_opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
  std::uint64_t kdf_memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
  std::string salt;
  std::string wrap_nonce;
  std::string stream_header;
  std::uint32_t chunk_size = kChunkSize;
  std::string wrapped_master_key;
};

struct ContainerVersion {
  std::uint32_t container_version = 0;
};

struct RecoveryEnvelopeForWrap {
  std::uint32_t envelope_version = kRecoveryEnvelopeVersion;
  std::uint32_t kdf_algorithm = crypto_pwhash_ALG_ARGON2ID13;
  std::uint64_t kdf_opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
  std::uint64_t kdf_memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
  std::string salt;
  std::string wrap_nonce;
};

CryptoResult failure(std::string error) {
  return {.ok = false, .error = std::move(error)};
}

CryptoResult success() {
  return {.ok = true, .error = {}};
}

bool sodiumReady() {
  static std::once_flag once;
  static bool ready = false;
  std::call_once(once, [] { ready = sodium_init() >= 0; });
  return ready;
}

template <std::size_t Size>
std::string encode(const std::array<unsigned char, Size> &bytes) {
  std::array<char, sodium_base64_ENCODED_LEN(Size, sodium_base64_VARIANT_ORIGINAL)>
      encoded{};
  sodium_bin2base64(encoded.data(), encoded.size(), bytes.data(), bytes.size(),
                    sodium_base64_VARIANT_ORIGINAL);
  return encoded.data();
}

template <std::size_t Size>
bool decode(std::string_view encoded, std::array<unsigned char, Size> *bytes) {
  std::size_t decodedSize = 0;
  const char *end = nullptr;
  if (sodium_base642bin(bytes->data(), bytes->size(), encoded.data(), encoded.size(),
                        nullptr, &decodedSize, &end,
                        sodium_base64_VARIANT_ORIGINAL) != 0 ||
      decodedSize != bytes->size() || end != encoded.data() + encoded.size()) {
    return false;
  }
  return true;
}

void appendUint32(std::string *bytes, std::uint32_t value) {
  for (int offset = 0; offset != 4; ++offset) {
    bytes->push_back(static_cast<char>((value >> (offset * 8)) & 0xff));
  }
}

bool readUint32(std::istream *input, std::uint32_t *value) {
  std::array<unsigned char, 4> bytes{};
  input->read(reinterpret_cast<char *>(bytes.data()), bytes.size());
  if (input->gcount() != static_cast<std::streamsize>(bytes.size())) {
    return false;
  }
  *value = static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
  return true;
}

RecoveryEnvelope envelopeFromHeader(const ContainerHeader &header) {
  return {.envelope_version = header.envelope_version,
          .kdf_algorithm = header.kdf_algorithm,
          .kdf_opslimit = header.kdf_opslimit,
          .kdf_memlimit = header.kdf_memlimit,
          .salt = header.salt,
          .wrap_nonce = header.wrap_nonce,
          .wrapped_master_key = header.wrapped_master_key};
}

RecoveryEnvelopeForWrap envelopeForWrap(const RecoveryEnvelope &envelope) {
  return {.envelope_version = envelope.envelope_version,
          .kdf_algorithm = envelope.kdf_algorithm,
          .kdf_opslimit = envelope.kdf_opslimit,
          .kdf_memlimit = envelope.kdf_memlimit,
          .salt = envelope.salt,
          .wrap_nonce = envelope.wrap_nonce};
}

std::string wrapAdditionalData(const RecoveryEnvelope &envelope) {
  return std::string{kMagic} + rfl::json::write(envelopeForWrap(envelope));
}

LegacyHeaderForWrap legacyHeaderForWrap(const LegacyContainerHeader &header) {
  return {.container_version = header.container_version,
          .kdf_algorithm = header.kdf_algorithm,
          .kdf_opslimit = header.kdf_opslimit,
          .kdf_memlimit = header.kdf_memlimit,
          .salt = header.salt,
          .wrap_nonce = header.wrap_nonce,
          .stream_header = header.stream_header,
          .chunk_size = header.chunk_size};
}

std::string legacyWrapAdditionalData(const LegacyContainerHeader &header) {
  return std::string{kMagic} + rfl::json::write(legacyHeaderForWrap(header));
}

bool validEnvelope(const RecoveryEnvelope &envelope) {
  std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
  std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> wrapNonce{};
  std::array<unsigned char,
             crypto_aead_xchacha20poly1305_ietf_ABYTES + MasterKey{}.bytes.size()>
      wrappedMasterKey{};
  return envelope.envelope_version == kRecoveryEnvelopeVersion &&
         envelope.kdf_algorithm == crypto_pwhash_ALG_ARGON2ID13 &&
         envelope.kdf_opslimit >= crypto_pwhash_OPSLIMIT_INTERACTIVE &&
         envelope.kdf_opslimit <= kMaxKdfOperations &&
         envelope.kdf_memlimit >= crypto_pwhash_MEMLIMIT_INTERACTIVE &&
         envelope.kdf_memlimit <= kMaxKdfMemory && decode(envelope.salt, &salt) &&
         decode(envelope.wrap_nonce, &wrapNonce) &&
         decode(envelope.wrapped_master_key, &wrappedMasterKey);
}

bool validLegacyHeader(const LegacyContainerHeader &header) {
  std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
  std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> wrapNonce{};
  std::array<unsigned char, crypto_secretstream_xchacha20poly1305_HEADERBYTES> streamHeader{};
  std::array<unsigned char,
             crypto_aead_xchacha20poly1305_ietf_ABYTES + MasterKey{}.bytes.size()>
      wrappedMasterKey{};
  return header.container_version == kLegacyContainerVersion &&
         header.kdf_algorithm == crypto_pwhash_ALG_ARGON2ID13 &&
         header.kdf_opslimit >= crypto_pwhash_OPSLIMIT_INTERACTIVE &&
         header.kdf_opslimit <= kMaxKdfOperations &&
         header.kdf_memlimit >= crypto_pwhash_MEMLIMIT_INTERACTIVE &&
         header.kdf_memlimit <= kMaxKdfMemory && decode(header.salt, &salt) &&
         decode(header.wrap_nonce, &wrapNonce) &&
         decode(header.stream_header, &streamHeader) && header.chunk_size > 0 &&
         header.chunk_size <= kChunkSize && decode(header.wrapped_master_key, &wrappedMasterKey);
}

bool validHeader(const ContainerHeader &header) {
  std::array<unsigned char, crypto_secretstream_xchacha20poly1305_HEADERBYTES> streamHeader{};
  return header.container_version == kCurrentContainerVersion &&
         validEnvelope(envelopeFromHeader(header)) && decode(header.stream_header, &streamHeader) &&
         header.chunk_size > 0 && header.chunk_size <= kChunkSize;
}

bool deriveWrappingKey(std::string_view password,
                       const std::array<unsigned char, crypto_pwhash_SALTBYTES> &salt,
                       std::uint64_t operations, std::uint64_t memory,
                       std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_KEYBYTES>
                           *key) {
  return crypto_pwhash(key->data(), key->size(), password.data(), password.size(),
                       salt.data(), operations, memory,
                       crypto_pwhash_ALG_ARGON2ID13) == 0;
}

std::string randomSuffix() {
  std::array<unsigned char, 16> bytes{};
  randombytes_buf(bytes.data(), bytes.size());
  std::array<char, 33> hex{};
  sodium_bin2hex(hex.data(), hex.size(), bytes.data(), bytes.size());
  return hex.data();
}

class TemporaryOutput {
 public:
  explicit TemporaryOutput(const std::string &outputPath)
      : path_(outputPath + ".partial-" + randomSuffix()) {}

  TemporaryOutput(const TemporaryOutput &) = delete;
  TemporaryOutput &operator=(const TemporaryOutput &) = delete;

  ~TemporaryOutput() {
    if (!published_) {
      std::error_code error;
      std::filesystem::remove(path_, error);
    }
  }

  const std::string &path() const { return path_; }

  bool publish(const std::string &outputPath) {
    try {
      // Poco uses MoveFileExW with MOVEFILE_REPLACE_EXISTING on Windows.
      Poco::File(path_).renameTo(outputPath);
      published_ = true;
      return true;
    } catch (const Poco::Exception &) {
      return false;
    }
  }

 private:
  std::string path_;
  bool published_ = false;
};

class SodiumMemzeroGuard {
 public:
  SodiumMemzeroGuard(unsigned char *data, std::size_t size) : data_(data), size_(size) {}

  template <std::size_t Size>
  explicit SodiumMemzeroGuard(std::array<unsigned char, Size> &data)
      : data_(data.data()), size_(data.size()) {}

  explicit SodiumMemzeroGuard(crypto_secretstream_xchacha20poly1305_state &state)
      : data_(reinterpret_cast<unsigned char *>(&state)), size_(sizeof(state)) {}

  SodiumMemzeroGuard(const SodiumMemzeroGuard &) = delete;
  SodiumMemzeroGuard &operator=(const SodiumMemzeroGuard &) = delete;

  ~SodiumMemzeroGuard() { sodium_memzero(data_, size_); }

 private:
  unsigned char *data_;
  std::size_t size_;
};

bool writeRecord(std::ostream *output, const unsigned char *ciphertext,
                 std::uint32_t size) {
  std::string encodedSize;
  appendUint32(&encodedSize, size);
  output->write(encodedSize.data(), encodedSize.size());
  output->write(reinterpret_cast<const char *>(ciphertext), size);
  return static_cast<bool>(*output);
}

bool decodeHeaderAndDeriveKey(
    const std::string &serialized_header, std::string_view recovery_password,
    std::array<unsigned char, crypto_pwhash_SALTBYTES> *salt,
    std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> *wrap_nonce,
    std::array<unsigned char, crypto_secretstream_xchacha20poly1305_HEADERBYTES> *stream_header,
    std::array<unsigned char,
               crypto_aead_xchacha20poly1305_ietf_ABYTES + MasterKey{}.bytes.size()>
        *wrapped_master_key,
    std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> *wrapping_key,
    std::uint32_t *chunk_size, std::string *wrap_additional_data) {
  const auto parsedVersion = rfl::json::read<ContainerVersion>(serialized_header);
  if (!parsedVersion) {
    return false;
  }
  if (parsedVersion.value().container_version == kLegacyContainerVersion) {
    const auto header = rfl::json::read<LegacyContainerHeader>(serialized_header);
    if (!header || !validLegacyHeader(header.value()) ||
        !decode(header.value().salt, salt) || !decode(header.value().wrap_nonce, wrap_nonce) ||
        !decode(header.value().stream_header, stream_header) ||
        !decode(header.value().wrapped_master_key, wrapped_master_key) ||
        !deriveWrappingKey(recovery_password, *salt, header.value().kdf_opslimit,
                           header.value().kdf_memlimit, wrapping_key)) {
      return false;
    }
    *chunk_size = header.value().chunk_size;
    *wrap_additional_data = legacyWrapAdditionalData(header.value());
    return true;
  }
  if (parsedVersion.value().container_version != kCurrentContainerVersion) {
    return false;
  }

  const auto header = rfl::json::read<ContainerHeader>(serialized_header);
  if (!header || !validHeader(header.value())) {
    return false;
  }
  const auto envelope = envelopeFromHeader(header.value());
  if (!decode(envelope.salt, salt) || !decode(envelope.wrap_nonce, wrap_nonce) ||
      !decode(header.value().stream_header, stream_header) ||
      !decode(envelope.wrapped_master_key, wrapped_master_key) ||
      !deriveWrappingKey(recovery_password, *salt, envelope.kdf_opslimit,
                         envelope.kdf_memlimit, wrapping_key)) {
    return false;
  }
  *chunk_size = header.value().chunk_size;
  *wrap_additional_data = wrapAdditionalData(envelope);
  return true;
}

CryptoResult decryptPayload(
    std::istream *input, const std::string &zip_path, const std::string &serialized_header,
    const std::array<unsigned char, crypto_secretstream_xchacha20poly1305_HEADERBYTES> &stream_header,
    std::uint32_t chunk_size, const MasterKey &master_key) {
  crypto_secretstream_xchacha20poly1305_state streamState{};
  SodiumMemzeroGuard streamStateGuard(streamState);
  if (crypto_secretstream_xchacha20poly1305_init_pull(&streamState, stream_header.data(),
                                                       master_key.bytes.data()) != 0) {
    return failure("cannot decrypt encrypted backup");
  }

  TemporaryOutput temporary(zip_path);
  std::ofstream output(temporary.path(), std::ios::binary | std::ios::trunc);
  if (!output) {
    return failure("cannot decrypt encrypted backup");
  }
  bool foundFinalRecord = false;
  std::vector<unsigned char> ciphertext(chunk_size +
                                         crypto_secretstream_xchacha20poly1305_ABYTES);
  std::vector<unsigned char> plaintext(chunk_size);
  while (!foundFinalRecord) {
    std::uint32_t cipherSize = 0;
    if (!readUint32(input, &cipherSize) ||
        cipherSize < crypto_secretstream_xchacha20poly1305_ABYTES ||
        cipherSize > ciphertext.size()) {
      return failure("cannot decrypt encrypted backup");
    }
    input->read(reinterpret_cast<char *>(ciphertext.data()), cipherSize);
    if (input->gcount() != static_cast<std::streamsize>(cipherSize)) {
      return failure("cannot decrypt encrypted backup");
    }
    unsigned long long plaintextSize = 0;
    unsigned char tag = 0;
    if (crypto_secretstream_xchacha20poly1305_pull(
            &streamState, plaintext.data(), &plaintextSize, &tag, ciphertext.data(), cipherSize,
            reinterpret_cast<const unsigned char *>(serialized_header.data()),
            serialized_header.size()) != 0 ||
        plaintextSize > plaintext.size() ||
        (tag != crypto_secretstream_xchacha20poly1305_TAG_MESSAGE &&
         tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL)) {
      return failure("cannot decrypt encrypted backup");
    }
    output.write(reinterpret_cast<const char *>(plaintext.data()), plaintextSize);
    if (!output) {
      return failure("cannot decrypt encrypted backup");
    }
    foundFinalRecord = tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL;
  }
  if (input->peek() != std::char_traits<char>::eof()) {
    return failure("cannot decrypt encrypted backup");
  }
  output.close();
  return output && temporary.publish(zip_path) ? success()
                                                : failure("cannot decrypt encrypted backup");
}

} // namespace

BackupContainerKind detect_backup_container(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return BackupContainerKind::Unknown;
  }

  std::array<char, kMagic.size()> magic{};
  input.read(magic.data(), magic.size());
  if (input.gcount() == static_cast<std::streamsize>(magic.size()) &&
      std::memcmp(magic.data(), kMagic.data(), kMagic.size()) == 0) {
    return BackupContainerKind::Encrypted;
  }
  if (magic[0] == 'P' && magic[1] == 'K') {
    return BackupContainerKind::PlainZip;
  }
  return BackupContainerKind::Unknown;
}

CryptoResult create_recovery_envelope(std::string_view recovery_password,
                                      const MasterKey &master_key,
                                      RecoveryEnvelope *envelope) {
  if (!sodiumReady()) {
    return failure("crypto library is unavailable");
  }
  if (envelope == nullptr) {
    return failure("recovery envelope output is required");
  }
  if (recovery_password.size() < 12) {
    return failure("recovery password must contain at least 12 characters");
  }

  std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
  std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> wrapNonce{};
  std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> wrappingKey{};
  std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_ABYTES + MasterKey{}.bytes.size()>
      wrappedMasterKey{};
  SodiumMemzeroGuard wrappingKeyGuard(wrappingKey.data(), wrappingKey.size());
  SodiumMemzeroGuard wrappedMasterKeyGuard(wrappedMasterKey.data(), wrappedMasterKey.size());
  randombytes_buf(salt.data(), salt.size());
  randombytes_buf(wrapNonce.data(), wrapNonce.size());

  RecoveryEnvelope created;
  created.envelope_version = kRecoveryEnvelopeVersion;
  created.kdf_algorithm = crypto_pwhash_ALG_ARGON2ID13;
  created.kdf_opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
  created.kdf_memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
  created.salt = encode(salt);
  created.wrap_nonce = encode(wrapNonce);
  if (!deriveWrappingKey(recovery_password, salt, created.kdf_opslimit,
                         created.kdf_memlimit, &wrappingKey)) {
    return failure("cannot derive backup encryption key");
  }

  const auto wrapAd = wrapAdditionalData(created);
  unsigned long long wrappedSize = 0;
  if (crypto_aead_xchacha20poly1305_ietf_encrypt(
          wrappedMasterKey.data(), &wrappedSize, master_key.bytes.data(),
          master_key.bytes.size(),
          reinterpret_cast<const unsigned char *>(wrapAd.data()), wrapAd.size(), nullptr,
          wrapNonce.data(), wrappingKey.data()) != 0 ||
      wrappedSize != wrappedMasterKey.size()) {
    return failure("cannot wrap backup encryption key");
  }
  created.wrapped_master_key = encode(wrappedMasterKey);
  *envelope = std::move(created);
  return success();
}

CryptoResult validate_recovery_envelope(const RecoveryEnvelope &envelope) {
  if (!sodiumReady()) {
    return failure("crypto library is unavailable");
  }
  return validEnvelope(envelope) ? success() : failure("invalid recovery envelope");
}

std::string serialize_recovery_envelope(const RecoveryEnvelope &envelope) {
  return rfl::json::write(envelope);
}

std::optional<RecoveryEnvelope>
deserialize_recovery_envelope(const std::string_view serialized_envelope) {
  const auto parsed = rfl::json::read<RecoveryEnvelope>(serialized_envelope);
  if (!parsed || !validEnvelope(parsed.value())) {
    return std::nullopt;
  }
  return parsed.value();
}

CryptoResult encrypt_backup_file(const std::string &zip_path,
                                 const std::string &output_path,
                                 std::string_view recovery_password,
                                 const MasterKey &master_key) {
  RecoveryEnvelope recoveryEnvelope;
  const auto created =
      create_recovery_envelope(recovery_password, master_key, &recoveryEnvelope);
  if (!created.ok) {
    return created;
  }
  return encrypt_backup_file(zip_path, output_path, recoveryEnvelope, master_key);
}

CryptoResult encrypt_backup_file(const std::string &zip_path,
                                 const std::string &output_path,
                                 const RecoveryEnvelope &recovery_envelope,
                                 const MasterKey &master_key) {
  if (!sodiumReady()) {
    return failure("crypto library is unavailable");
  }
  if (!validEnvelope(recovery_envelope)) {
    return failure("invalid recovery envelope");
  }

  std::ifstream input(zip_path, std::ios::binary);
  if (!input) {
    return failure("cannot read backup archive");
  }

  std::array<unsigned char, crypto_secretstream_xchacha20poly1305_HEADERBYTES> streamHeader{};
  ContainerHeader header{
      .container_version = kCurrentContainerVersion,
      .envelope_version = recovery_envelope.envelope_version,
      .kdf_algorithm = recovery_envelope.kdf_algorithm,
      .kdf_opslimit = recovery_envelope.kdf_opslimit,
      .kdf_memlimit = recovery_envelope.kdf_memlimit,
      .salt = recovery_envelope.salt,
      .wrap_nonce = recovery_envelope.wrap_nonce,
      .stream_header = {},
      .chunk_size = kChunkSize,
      .wrapped_master_key = recovery_envelope.wrapped_master_key,
  };

  crypto_secretstream_xchacha20poly1305_state streamState{};
  SodiumMemzeroGuard streamStateGuard(streamState);
  if (crypto_secretstream_xchacha20poly1305_init_push(&streamState, streamHeader.data(),
                                                       master_key.bytes.data()) != 0) {
    return failure("cannot initialize backup encryption");
  }
  header.stream_header = encode(streamHeader);
  const auto serializedHeader = rfl::json::write(header);
  if (serializedHeader.size() > kMaxHeaderSize) {
    return failure("encrypted backup header is too large");
  }

  TemporaryOutput temporary(output_path);
  std::ofstream output(temporary.path(), std::ios::binary | std::ios::trunc);
  if (!output) {
    return failure("cannot create encrypted backup");
  }
  output.write(kMagic.data(), kMagic.size());
  std::string encodedHeaderSize;
  appendUint32(&encodedHeaderSize, static_cast<std::uint32_t>(serializedHeader.size()));
  output.write(encodedHeaderSize.data(), encodedHeaderSize.size());
  output.write(serializedHeader.data(), serializedHeader.size());
  if (!output) {
    return failure("cannot write encrypted backup header");
  }

  std::vector<unsigned char> plaintext(header.chunk_size);
  std::vector<unsigned char> ciphertext(header.chunk_size +
                                         crypto_secretstream_xchacha20poly1305_ABYTES);
  while (true) {
    input.read(reinterpret_cast<char *>(plaintext.data()), plaintext.size());
    const auto readSize = static_cast<unsigned long long>(input.gcount());
    if (input.bad() || (!input.eof() && input.fail())) {
      return failure("cannot read backup archive");
    }

    const auto tag = input.eof() ? crypto_secretstream_xchacha20poly1305_TAG_FINAL :
                                   crypto_secretstream_xchacha20poly1305_TAG_MESSAGE;
    unsigned long long cipherSize = 0;
    if (crypto_secretstream_xchacha20poly1305_push(
            &streamState, ciphertext.data(), &cipherSize, plaintext.data(), readSize,
            reinterpret_cast<const unsigned char *>(serializedHeader.data()),
            serializedHeader.size(), tag) != 0 ||
        cipherSize > std::numeric_limits<std::uint32_t>::max() ||
        !writeRecord(&output, ciphertext.data(), static_cast<std::uint32_t>(cipherSize))) {
      return failure("cannot write encrypted backup");
    }
    if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
      break;
    }
  }

  output.close();
  if (!output || !temporary.publish(output_path)) {
    return failure("cannot finalize encrypted backup");
  }
  return success();
}

CryptoResult decrypt_backup_file(const std::string &input_path,
                                 const std::string &zip_path,
                                 std::string_view recovery_password,
                                 MasterKey *key_from_password) {
  if (!sodiumReady()) {
    return failure("crypto library is unavailable");
  }
  std::ifstream input(input_path, std::ios::binary);
  if (!input) {
    return failure("cannot decrypt encrypted backup");
  }

  std::array<char, kMagic.size()> magic{};
  input.read(magic.data(), magic.size());
  if (input.gcount() != static_cast<std::streamsize>(magic.size()) ||
      std::memcmp(magic.data(), kMagic.data(), kMagic.size()) != 0) {
    return failure("cannot decrypt encrypted backup");
  }
  std::uint32_t headerSize = 0;
  if (!readUint32(&input, &headerSize) || headerSize == 0 || headerSize > kMaxHeaderSize) {
    return failure("cannot decrypt encrypted backup");
  }
  std::string serializedHeader(headerSize, '\0');
  input.read(serializedHeader.data(), serializedHeader.size());
  if (input.gcount() != static_cast<std::streamsize>(serializedHeader.size())) {
    return failure("cannot decrypt encrypted backup");
  }
  std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
  std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES> wrapNonce{};
  std::array<unsigned char, crypto_secretstream_xchacha20poly1305_HEADERBYTES> streamHeader{};
  std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_ABYTES + MasterKey{}.bytes.size()>
      wrappedMasterKey{};
  std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> wrappingKey{};
  MasterKey masterKey;
  SodiumMemzeroGuard wrappingKeyGuard(wrappingKey.data(), wrappingKey.size());
  SodiumMemzeroGuard wrappedMasterKeyGuard(wrappedMasterKey.data(), wrappedMasterKey.size());
  SodiumMemzeroGuard masterKeyGuard(masterKey.bytes.data(), masterKey.bytes.size());
  std::uint32_t chunkSize = 0;
  std::string wrapAd;

  if (!decodeHeaderAndDeriveKey(serializedHeader, recovery_password, &salt, &wrapNonce,
                                &streamHeader, &wrappedMasterKey, &wrappingKey, &chunkSize,
                                &wrapAd) ||
      chunkSize == 0) {
    return failure("cannot decrypt encrypted backup");
  }

  unsigned long long keySize = 0;
  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          masterKey.bytes.data(), &keySize, nullptr, wrappedMasterKey.data(),
          wrappedMasterKey.size(),
          reinterpret_cast<const unsigned char *>(wrapAd.data()), wrapAd.size(),
          wrapNonce.data(), wrappingKey.data()) != 0 ||
      keySize != masterKey.bytes.size()) {
    return failure("cannot decrypt encrypted backup");
  }

  const auto result = decryptPayload(&input, zip_path, serializedHeader, streamHeader, chunkSize,
                                     masterKey);
  if (!result.ok) {
    return result;
  }
  if (key_from_password != nullptr) {
    *key_from_password = masterKey;
  }
  return success();
}

} // namespace pcm::backup
