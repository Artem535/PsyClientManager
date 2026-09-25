#include "crypto/livekit_jwt.h"

#include <sodium.h>

#include <chrono>
#include <sstream>
#include <stdexcept>

namespace pcm::tokenbackend {

namespace {

constexpr char kBase64UrlAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string base64UrlEncode(const unsigned char *data, size_t len) {
  std::string out;
  out.reserve((len * 4 + 2) / 3);
  size_t i = 0;
  while (i + 3 <= len) {
    uint32_t triple = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
    out += kBase64UrlAlphabet[(triple >> 18) & 0x3F];
    out += kBase64UrlAlphabet[(triple >> 12) & 0x3F];
    out += kBase64UrlAlphabet[(triple >> 6) & 0x3F];
    out += kBase64UrlAlphabet[triple & 0x3F];
    i += 3;
  }
  size_t remaining = len - i;
  if (remaining == 1) {
    uint32_t val = data[i] << 16;
    out += kBase64UrlAlphabet[(val >> 18) & 0x3F];
    out += kBase64UrlAlphabet[(val >> 12) & 0x3F];
  } else if (remaining == 2) {
    uint32_t val = (data[i] << 16) | (data[i + 1] << 8);
    out += kBase64UrlAlphabet[(val >> 18) & 0x3F];
    out += kBase64UrlAlphabet[(val >> 12) & 0x3F];
    out += kBase64UrlAlphabet[(val >> 6) & 0x3F];
  }
  return out;
}

std::string base64UrlEncode(const std::string &s) {
  return base64UrlEncode(reinterpret_cast<const unsigned char *>(s.data()), s.size());
}

std::string jsonEscape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

} // namespace

std::string mintLiveKitJwt(const std::string &apiKey, const std::string &apiSecret,
                            const std::string &identity, const VideoGrants &grants,
                            int ttlSeconds) {
  if (apiKey.empty() || apiSecret.empty() || identity.empty() || grants.room.empty()) {
    throw std::invalid_argument("apiKey, apiSecret, identity, and grants.room are required");
  }

  auto now = std::chrono::system_clock::now();
  auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  auto expSeconds = nowSeconds + ttlSeconds;

  std::ostringstream header;
  header << R"({"alg":"HS256","typ":"JWT"})";

  std::ostringstream payload;
  payload << "{"
          << "\"iss\":\"" << jsonEscape(apiKey) << "\","
          << "\"sub\":\"" << jsonEscape(identity) << "\","
          << "\"nbf\":" << nowSeconds << ","
          << "\"exp\":" << expSeconds << ","
          << "\"video\":{"
          << "\"room\":\"" << jsonEscape(grants.room) << "\","
          << "\"roomJoin\":" << (grants.roomJoin ? "true" : "false") << ","
          << "\"canPublish\":" << (grants.canPublish ? "true" : "false") << ","
          << "\"canSubscribe\":" << (grants.canSubscribe ? "true" : "false") << ","
          << "\"canPublishData\":" << (grants.canPublishData ? "true" : "false")
          << "}"
          << "}";

  std::string signingInput = base64UrlEncode(header.str()) + "." + base64UrlEncode(payload.str());

  unsigned char signature[crypto_auth_hmacsha256_BYTES];
  crypto_auth_hmacsha256(signature, reinterpret_cast<const unsigned char *>(signingInput.data()),
                          signingInput.size(),
                          reinterpret_cast<const unsigned char *>(apiSecret.data()));

  return signingInput + "." + base64UrlEncode(signature, sizeof(signature));
}

} // namespace pcm::tokenbackend
