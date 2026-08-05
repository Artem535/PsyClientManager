#include "app_lock_service.h"

#include <sodium.h>

#include <array>
#include <algorithm>

namespace pcm {
namespace {

constexpr auto kSaltKey = "privacy/appLockSalt";
constexpr auto kVerifierKey = "privacy/appLockVerifier";
constexpr std::size_t kVerifierBytes = 32;

bool sodiumReady() {
  static const bool ready = sodium_init() >= 0;
  return ready;
}

} // namespace

AppLockService::AppLockService(QSettings *settings)
    : mSettings(settings != nullptr ? settings : &mDefaultSettings) {}

bool AppLockService::configure(const std::string_view credential) {
  if (!sodiumReady() || !isValidCredential(credential)) {
    return false;
  }
  std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
  std::array<unsigned char, kVerifierBytes> verifier{};
  randombytes_buf(salt.data(), salt.size());
  if (crypto_pwhash(verifier.data(), verifier.size(), credential.data(), credential.size(),
                    salt.data(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                    crypto_pwhash_MEMLIMIT_INTERACTIVE,
                    crypto_pwhash_ALG_ARGON2ID13) != 0) {
    sodium_memzero(salt.data(), salt.size());
    sodium_memzero(verifier.data(), verifier.size());
    return false;
  }
  mSettings->setValue(
      kSaltKey,
      QByteArray(reinterpret_cast<const char *>(salt.data()), salt.size()).toBase64());
  mSettings->setValue(
      kVerifierKey,
      QByteArray(reinterpret_cast<const char *>(verifier.data()), verifier.size()).toBase64());
  sodium_memzero(salt.data(), salt.size());
  sodium_memzero(verifier.data(), verifier.size());
  return true;
}

bool AppLockService::verify(const std::string_view credential) const {
  if (!sodiumReady() || !isConfigured()) {
    return false;
  }
  const auto salt = QByteArray::fromBase64(mSettings->value(kSaltKey).toByteArray());
  const auto verifier = QByteArray::fromBase64(mSettings->value(kVerifierKey).toByteArray());
  if (salt.size() != crypto_pwhash_SALTBYTES || verifier.size() != kVerifierBytes) {
    return false;
  }
  std::array<unsigned char, kVerifierBytes> candidate{};
  const auto derived = crypto_pwhash(
      candidate.data(), candidate.size(), credential.data(), credential.size(),
      reinterpret_cast<const unsigned char *>(salt.constData()),
      crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
      crypto_pwhash_ALG_ARGON2ID13);
  const bool valid = derived == 0 &&
                     sodium_memcmp(candidate.data(), verifier.constData(), candidate.size()) == 0;
  sodium_memzero(candidate.data(), candidate.size());
  return valid;
}

bool AppLockService::isConfigured() const {
  return QByteArray::fromBase64(mSettings->value(kSaltKey).toByteArray()).size() ==
             crypto_pwhash_SALTBYTES &&
         QByteArray::fromBase64(mSettings->value(kVerifierKey).toByteArray()).size() ==
             kVerifierBytes;
}

void AppLockService::disable() {
  mSettings->remove(kSaltKey);
  mSettings->remove(kVerifierKey);
}

bool AppLockService::isValidCredential(const std::string_view credential) const {
  const bool pin = credential.size() >= 6 &&
                   std::ranges::all_of(credential, [](const char character) {
                     return character >= '0' && character <= '9';
                   });
  return pin || credential.size() >= 12;
}

} // namespace pcm
