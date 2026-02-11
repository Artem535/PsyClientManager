// app_database.h
#pragma once
#include <duckdb.hpp>
#define PLOG_NO_LOG_MACROS

#include <memory>
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

  int64_t add_event(const ObxEvent &event);
  bool update_event(const ObxEvent &event);
  std::unique_ptr<ObxEvent> get_event(const int64_t &id);
  bool remove_event(const int64_t &id);

  int64_t add_client(const ObxClient &client);
  bool remove_client(const int64_t &id);
  std::unique_ptr<ObxClient> get_client(const int64_t &id);
  std::vector<std::unique_ptr<ObxClient>> get_clients();
  std::vector<int64_t> get_client_ids();

  int64_t add_event_client(const int64_t &event_id, const int64_t &client_id);

  // std::vector<int64_t> get_event_ids(int64_t date);

  bool has_conflict(const ObxEvent &event);
  std::vector<ObxEvent> get_day_events(const int64_t &date_ms);

  ObxClient get_client_by_event(const int64_t &event_id);

private:
  void add_demo_data();
  void init_tables();
  void init_payment_status_table();
  void init_event_status_table();

  std::unique_ptr<duckdb::DuckDB> mDb;
};

} // namespace pcm::database
