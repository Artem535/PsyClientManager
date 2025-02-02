module;

#define OBX_CPP_FILE // Put this define in one file only before including
#include "objectbox-model.h"
#include "objectbox.hpp"
#include "scheme.obx.hpp"

#include <Poco/File.h>

#include <memory>
#include <vector>

export module app_database;

import config;
import database_constance;

namespace pcm_conf = pcm::config;

namespace pcm::database {

export class Database {

public:
  explicit Database(const pcm_conf::Config &conf) {
    auto db_pth = conf.db_conf.value_.db_pth;

    bool it_first_init = false;
    if (auto dir = Poco::File(db_pth); !dir.exists()) {
      // Make directory can be used only if db_pth not const.
      dir.createDirectories();
      it_first_init = true;
    }

    auto options = obx::Options(create_obx_model());
    options.directory(db_pth.toString());

    m_store = std::make_unique<obx::Store>(options);
    m_events_box = std::make_unique<obx::Box<Events>>(*m_store.get());
    m_clinet_box = std::make_unique<obx::Box<Client>>(*m_store.get());
    m_payment_status_box =
        std::make_unique<obx::Box<PaymentStatus>>(*m_store.get());
    m_event_status_box =
        std::make_unique<obx::Box<EventStatus>>(*m_store.get());

    if (it_first_init) {
      init_payment_status_table();
      init_event_status_table();
    }
  }

  // Events
  obx_id add_event(const Events &event) { return m_events_box->put(event); }
  bool remove_event(const obx_id &id) { return m_events_box->remove(id); }

  // Client
  obx_id add_client(const Client &client) { return m_clinet_box->put(client); }
  bool remove_client(const obx_id &id) { return m_clinet_box->remove(id); }
  std::vector<std::unique_ptr<Client>> get_clients() {
    return m_clinet_box->getAll();
  }

private:
  // PaymentStatus
  void init_payment_status_table() {
    for (const auto &status : constance::payment_statuses)
      m_payment_status_box->put(status);
  }

  // EventStatus
  void init_event_status_table() {
    for (const auto &status : constance::event_statuses)
      m_event_status_box->put(status);
  }

private:
  std::unique_ptr<obx::Store> m_store;
  std::unique_ptr<obx::Box<Events>> m_events_box;
  std::unique_ptr<obx::Box<Client>> m_clinet_box;
  std::unique_ptr<obx::Box<PaymentStatus>> m_payment_status_box;
  std::unique_ptr<obx::Box<EventStatus>> m_event_status_box;
};

} // namespace pcm::database
