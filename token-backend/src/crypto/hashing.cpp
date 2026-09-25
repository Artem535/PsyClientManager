#include "crypto/hashing.h"

#include <sodium.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace pcm::tokenbackend {

std::string fastHash(const std::string &plaintext) {
  unsigned char out[crypto_generichash_BYTES];
  crypto_generichash(out, sizeof(out),
                      reinterpret_cast<const unsigned char *>(plaintext.data()),
                      plaintext.size(), nullptr, 0);

  std::ostringstream oss;
  for (unsigned char byte : out) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
  }
  return oss.str();
}

bool fastHashMatches(const std::string &plaintext, const std::string &storedHashHex) {
  auto computed = fastHash(plaintext);
  if (computed.size() != storedHashHex.size()) {
    return false;
  }
  return sodium_memcmp(computed.data(), storedHashHex.data(), computed.size()) == 0;
}

std::string hashPasscode(const std::string &passcode) {
  char out[crypto_pwhash_STRBYTES];
  if (crypto_pwhash_str(out, passcode.c_str(), passcode.size(),
                         crypto_pwhash_OPSLIMIT_MODERATE,
                         crypto_pwhash_MEMLIMIT_MODERATE) != 0) {
    throw std::runtime_error("passcode hashing failed (out of memory)");
  }
  return std::string(out);
}

bool passcodeMatches(const std::string &passcode, const std::string &storedHash) {
  return crypto_pwhash_str_verify(storedHash.c_str(), passcode.c_str(), passcode.size()) == 0;
}

} // namespace pcm::tokenbackend
