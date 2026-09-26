#pragma once

#include <optional>
#include <string>

namespace pcm::meeting {

enum class ProviderKind { ExternalUrl, LiveKit };

[[nodiscard]] inline std::string providerKindToString(const ProviderKind kind) {
  switch (kind) {
  case ProviderKind::ExternalUrl:
    return "ExternalUrl";
  case ProviderKind::LiveKit:
    return "LiveKit";
  }
  return "ExternalUrl";
}

[[nodiscard]] inline std::optional<ProviderKind>
providerKindFromString(const std::string &value) {
  if (value == "ExternalUrl") {
    return ProviderKind::ExternalUrl;
  }
  if (value == "LiveKit") {
    return ProviderKind::LiveKit;
  }
  return std::nullopt;
}

} // namespace pcm::meeting
