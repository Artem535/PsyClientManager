module;

#include <Poco/File.h>
#include <Poco/Path.h>
#include <rfl/Flatten.hpp>
#include <rfl/Skip.hpp>
#include <rfl/default.hpp>
#include <rfl/yaml.hpp>
#include <rfl/yaml/load.hpp>
#include <rfl/yaml/save.hpp>

export module config;

import rfl_path;

export namespace pcm::config {

struct DatabaseConfig {
  Poco::Path db_pth = Poco::Path(Poco::Path::configHome())
                          .append("PsyClientManager")
                          .append("database");
};

struct Config {
  rfl::Skip<Poco::Path> config_pth = Poco::Path(Poco::Path::configHome())
                                         .append("PsyClientManager")
                                         .append("Config.yaml");

  rfl::Flatten<DatabaseConfig> db_conf;

  static void save_config(const Config &conf) {
    rfl::yaml::save(conf.config_pth.value().toString(), conf);
  }

  // TODO: Made this function inline?
  static Config read_config() {
    const auto default_pth = Config().config_pth.value();
    return rfl::yaml::load<Config>(default_pth.toString()).value();
  }
};

} // namespace pcm::config