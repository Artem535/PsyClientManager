#pragma once

#include <cstdint>
#include <string>

namespace pcm::tokenbackend {

struct Config {
  uint16_t port;
  std::string dbPath;
  std::string liveKitApiKey;
  std::string liveKitApiSecret;
  int tokenTtlSeconds;

  static Config fromEnv();
};

} // namespace pcm::tokenbackend
