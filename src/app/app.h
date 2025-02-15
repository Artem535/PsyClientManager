#pragma once

#include <memory>
#include "database.h"
#include "config.h"

namespace pcm {

class Application {
public:
    Application();
    int run();

private:
    std::shared_ptr<database::Database> m_db;
    config::Config m_conf;
};

} // namespace pcm