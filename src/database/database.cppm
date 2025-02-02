module;

#define OBX_CPP_FILE // Put this define in one file only before including
#include "objectbox-model.h"
#include "objectbox.hpp"
#include "scheme.obx.hpp"

#include <Poco/File.h>
#include <memory>

export module app_database;

import config;

namespace pcm_conf = pcm::config;

namespace pcm::database {

export class Database {

public:
  explicit Database(const pcm_conf::Config &conf) {
    auto db_pth = conf.db_conf.value_.db_pth;

    if (auto dir = Poco::File(db_pth); !dir.exists())
      // Make directory can be used only if db_pth not const.
      dir.createDirectories();

    auto options = obx::Options(create_obx_model());
    options.directory(db_pth.toString());

    m_store = std::make_unique<obx::Store>(options);
    m_events_box = std::make_unique<obx::Box<Events>>(*m_store.get());
  }

private:
  std::unique_ptr<obx::Store> m_store;
  std::unique_ptr<obx::Box<Events>> m_events_box;
};

} // namespace pcm::database
