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

struct ClientMonthlyStats {
  int year = 0;
  int month = 0;
  int sessions = 0;
  double income = 0.0;
};

struct DashboardSummary {
  int total_clients = 0;
  int active_clients = 0;
  int sessions_this_month = 0;
  int work_sessions_this_month = 0;
  int personal_sessions_this_month = 0;
  double income_this_month = 0.0;
};

struct DashboardMonthlyStats {
  int year = 0;
  int month = 0;
  int sessions = 0;
  int work_sessions = 0;
  int personal_sessions = 0;
  double income = 0.0;
};

class Database {
public:
  explicit Database(const pcm::config::Config &conf);

  int64_t add_event(const DuckEvent &event, bool allowOverlap = true);
  bool update_event(const DuckEvent &event, bool allowOverlap = true);
  std::unique_ptr<DuckEvent> get_event(const int64_t &id);
  bool remove_event(const int64_t &id);

  int64_t add_client(const DuckClient &client);
  bool remove_client(const int64_t &id);
  std::unique_ptr<DuckClient> get_client(const int64_t &id);
  std::vector<std::unique_ptr<DuckClient>> get_clients();
  std::vector<int64_t> get_client_ids();

  int64_t add_event_client(const int64_t &event_id, const int64_t &client_id);

  // std::vector<int64_t> get_event_ids(int64_t date);

  bool has_conflict(const DuckEvent &event);
  std::vector<DuckEvent> get_day_events(const int64_t &start_ms,
                                       const int64_t &end_ms);
  std::vector<ClientMonthlyStats> get_client_monthly_stats(const int64_t &client_id,
                                                           int months_back = 6);
  DashboardSummary get_dashboard_summary();
  std::vector<DashboardMonthlyStats> get_dashboard_monthly_stats(int months_back = 6);

  DuckClient get_client_by_event(const int64_t &event_id);

private:
  void add_demo_data();
  void init_tables();
  void apply_schema_migrations();
  void init_payment_status_table();
  void init_event_status_table();

  std::unique_ptr<duckdb::DuckDB> mDb;
};

} // namespace pcm::database
