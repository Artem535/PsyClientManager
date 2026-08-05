#include "app_lock_service.h"

#include <sodium.h>

#include <array>
#include <algorithm>

namespace pcm {
namespace {

constexpr auto kSaltKey = "privacy/appLockSalt";
constexpr auto kVerifierKey = "privacy/appLockVerifier";

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
  std::array<unsigned char, crypto_pwhash_STRBYTES> verifier{};
  randombytes_buf(salt.data(), salt.size());
  if (crypto_pwhash_str_alg(reinterpret_cast<char *>(verifier.data()), credential.data(),
                            credential.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                            crypto_pwhash_MEMLIMIT_INTERACTIVE,
                            crypto_pwhash_ALG_ARGON2ID13) != 0) {
    sodium_memzero(verifier.data(), verifier.size());
    return false;
  }
  mSettings->setValue(kSaltKey, QByteArray(reinterpret_cast<const char *>(salt.data()), salt.size()).toBase64());
  mSettings->setValue(
      kVerifierKey,
      QByteArray(reinterpret_cast<const char *>(verifier.data()), verifier.size()).toBase64());
  sodium_memzero(verifier.data(), verifier.size());
  return true;
}

bool AppLockService::verify(const std::string_view credential) const {
  if (!sodiumReady() || !isConfigured()) {
    return false;
  }
  const auto verifier = QByteArray::fromBase64(mSettings->value(kVerifierKey).toByteArray());
  return verifier.size() == crypto_pwhash_STRBYTES &&
         crypto_pwhash_str_verify(verifier.constData(), credential.data(), credential.size()) == 0;
}

bool AppLockService::isConfigured() const {
  return !mSettings->value(kSaltKey).toByteArray().isEmpty() &&
         !mSettings->value(kVerifierKey).toByteArray().isEmpty();
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
