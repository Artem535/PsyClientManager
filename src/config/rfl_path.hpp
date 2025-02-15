#pragma once

#include <Poco/Path.h>
#include <rfl/default.hpp>
#include <rfl/yaml.hpp>
#include <string>

namespace rfl {
template <> struct Reflector<Poco::Path> {
  using ReflType = std::string;

  static Poco::Path to(const ReflType &str) noexcept { return Poco::Path(str); }

  static ReflType from(const Poco::Path &v) { return v.toString(); }
};
} // namespace rfl
