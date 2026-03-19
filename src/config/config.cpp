#include "config.h"

namespace pcm::config {

void Config::save_config(const Config &conf) {
    rfl::yaml::save(conf.config_pth.value().toString(), conf);
}

Config Config::read_config() {
    const auto default_pth = Config().config_pth.value();
    return rfl::yaml::load<Config>(default_pth.toString()).value();
}

} // namespace pcm::config
