// app_database.h
#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <objectbox.h>
#include <ranges>
#include <vector>

#include "datetime.hpp"
#include "objectbox-model.h"
#include "objectbox.hpp"
#include "scheme.obx.hpp"

#include "config.h"
#include "constants.hpp"
#include "plog/Initializers/RollingFileInitializer.h"
#include <Poco/File.h>
#include <Poco/Timestamp.h>
#include <plog/Log.h>

namespace pcm::database {

class Database {
public:
  explicit Database(const pcm::config::Config &conf);

  obx_id add_event(const ObxEvent &event);
  std::unique_ptr<ObxEvent> get_event(const obx_id &id);
  bool remove_event(const obx_id &id);

  obx_id add_client(const ObxClient &client);
  bool remove_client(const obx_id &id);

  obx_id add_event_client(const obx_id &event_id, const obx_id &client_id);

  std::unique_ptr<ObxClient> get_client(const obx_id &id);
  std::vector<std::unique_ptr<ObxClient>> get_clients();

  std::vector<obx_id> get_client_ids();
  std::vector<obx_id> get_event_ids(int64_t date);


  bool has_conflict(const ObxEvent &event);
  std::vector<ObxEvent> get_day_events(const int64_t &date);

  ObxClient get_client_by_event(const obx_id &event_id);

private:
  void add_demo_data();
  void init_payment_status_table();
  void init_event_status_table();

  std::unique_ptr<obx::Store> m_store;
  std::unique_ptr<obx::Box<ObxEvent>> m_events_box;
  std::unique_ptr<obx::Box<ObxClient>> m_client_box;
  std::unique_ptr<obx::Box<ObxPaymentStatus>> m_payment_status_box;
  std::unique_ptr<obx::Box<ObxEventStatus>> m_event_status_box;
  std::unique_ptr<obx::Box<ObxEventClient>> m_event_client_box;
};

} // namespace pcm::database