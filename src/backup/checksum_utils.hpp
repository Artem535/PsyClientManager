// checksum_utils.hpp
#pragma once

#include <Poco/DigestEngine.h>
#include <Poco/SHA2Engine.h>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>

namespace pcm::backup {

inline std::string sha256_file(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("sha256_file: cannot open file: " + path);
  }

  Poco::SHA2Engine engine(Poco::SHA2Engine::SHA_256);
  std::array<char, 8192> buffer{};
  for (;;) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto n = in.gcount();
    if (n > 0) {
      engine.update(buffer.data(), static_cast<std::size_t>(n));
    }
    if (!in) {
      break;
    }
  }

  return Poco::DigestEngine::digestToHex(engine.digest());
}

} // namespace pcm::backup
