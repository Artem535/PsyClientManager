#pragma once

#include <Poco/Path.h>
#include <rfl/Flatten.hpp>
#include <rfl/default.hpp>
#include <rfl/Skip.hpp>
#include <rfl/yaml.hpp>
#include <rfl/yaml/load.hpp>
#include <rfl/yaml/save.hpp>
#include "rfl_path.hpp"

namespace pcm::config {

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

    static void save_config(const Config &conf);
    static Config read_config();
};

} // namespace pcm::config