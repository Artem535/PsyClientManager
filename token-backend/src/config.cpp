#include "config.h"

#include <cstdlib>
#include <stdexcept>

namespace pcm::tokenbackend {

namespace {
std::string requireEnv(const char *name) {
  const char *value = std::getenv(name);
  if (!value || value[0] == '\0') {
    throw std::runtime_error(std::string("missing required environment variable ") + name);
  }
  return value;
}
} // namespace

Config Config::fromEnv() {
  Config config;
  const char *portEnv = std::getenv("PORT");
  config.port = portEnv ? static_cast<uint16_t>(std::atoi(portEnv)) : 8080;

  const char *dbPathEnv = std::getenv("DB_PATH");
  config.dbPath = dbPathEnv ? dbPathEnv : "token-backend.sqlite3";

  config.liveKitApiKey = requireEnv("LIVEKIT_API_KEY");
  config.liveKitApiSecret = requireEnv("LIVEKIT_API_SECRET");

  const char *wsEndpointEnv = std::getenv("LIVEKIT_WS_ENDPOINT");
  config.liveKitWsEndpoint =
      (wsEndpointEnv && wsEndpointEnv[0] != '\0') ? wsEndpointEnv : "ws://46.173.25.218:7880";

  // Required rather than defaulted: the previous fallback of
  // "https://example.invalid/join/" meant a deploy that forgot to set this
  // came up healthy and handed out invitation links that go nowhere, with no
  // error in any log. Failing at startup is the only way the operator finds
  // out at all.
  config.invitationBaseUrl = requireEnv("INVITATION_BASE_URL");

  const char *ttlEnv = std::getenv("TOKEN_TTL_SECONDS");
  config.tokenTtlSeconds = ttlEnv ? std::atoi(ttlEnv) : 600;

  return config;
}

} // namespace pcm::tokenbackend
