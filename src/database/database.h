// app_database.h
#pragma once
#include <duckdb.hpp>
#define PLOG_NO_LOG_MACROS

#include <memory>
#include <objectbox.h>
#include <vector>

#include "config.h"
#include "constants.hpp"
#include "datetime.hpp"
#include "db_utils.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "plog/Initializers/RollingFileInitializer.h"
#include "schema.hpp"
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
  std::unique_ptr<ObxClient> get_client(const obx_id &id);
  std::vector<std::unique_ptr<ObxClient>> get_clients();
  std::vector<obx_id> get_client_ids();

  obx_id add_event_client(const obx_id &event_id, const obx_id &client_id);

  // std::vector<obx_id> get_event_ids(int64_t date);

  bool has_conflict(const ObxEvent &event);
  std::vector<ObxEvent> get_day_events(const int64_t &date_microseconds);

  ObxClient get_client_by_event(const obx_id &event_id);

private:
  void add_demo_data();
  void init_tables();
  void init_payment_status_table();
  void init_event_status_table();

  std::unique_ptr<duckdb::DuckDB> mDb;
};

} // namespace pcm::database