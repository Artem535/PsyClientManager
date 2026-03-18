// db_structs.hpp
#pragma once
#include "db_utils.hpp"
#include <cstdint>
#include <duckdb.hpp>
#include <optional>
#include <ostream>
#include <string>

using duckdb::DataChunk;
using duckdb::idx_t;
using duckdb::timestamp_t;
using namespace pcm::database;

template <typename T>
inline std::ostream &print_optional(std::ostream &os,
                                    const std::optional<T> &opt) {
  if (opt.has_value()) {
    os << *opt;
  } else {
    os << "nullopt";
  }
  return os;
}

// --- DuckPaymentStatus ---
struct DuckPaymentStatus {
  std::int64_t id = -1;
  std::string name;
  DuckPaymentStatus() = default;
  DuckPaymentStatus(const duckdb::DataChunk &chunk, duckdb::idx_t index) {
    auto id_val = chunk.GetValue(0, index);
    auto name_val = chunk.GetValue(1, index);
    id = db_utils::toInt32AsInt64(id_val);
    name = name_val.ToString(); // NOT NULL in schema
  }
};
inline std::ostream &operator<<(std::ostream &os, const DuckPaymentStatus &s) {
  os << "DuckPaymentStatus{id=" << s.id << ", name=\"" << s.name << "\"}";
  return os;
}
// --- DuckEventStatus ---
struct DuckEventStatus {
  std::int64_t id = -1;
  std::string name;
  DuckEventStatus() = default;
  DuckEventStatus(const DataChunk &chunk, idx_t index) {
    id = db_utils::toInt32AsInt64(chunk.GetValue(0, index));
    name = chunk.GetValue(1, index).ToString();
  }
};
inline std::ostream &operator<<(std::ostream &os, const DuckEventStatus &s) {
  os << "DuckEventStatus{id=" << s.id << ", name=\"" << s.name << "\"}";
  return os;
}
// --- DuckClient ---
struct DuckClient {
  std::int64_t id = -1;
  std::optional<std::string> name = std::nullopt;
  std::optional<std::string> last_name = std::nullopt;
  std::optional<std::string> additional_info = std::nullopt;
  std::optional<std::string> diagnosis = std::nullopt;
  std::optional<std::int64_t> birthday_date = std::nullopt;
  std::optional<std::string> email = std::nullopt;
  std::optional<std::string> phone_number = std::nullopt;
  bool client_active = false;
  std::optional<std::string> country = std::nullopt;
  std::optional<std::string> city = std::nullopt;
  std::optional<std::string> time_zone = std::nullopt;
  DuckClient() = default;
  DuckClient(const duckdb::DataChunk &chunk, duckdb::idx_t index) {
    id = db_utils::toInt32AsInt64(chunk.GetValue(0, index));
    name = db_utils::toOptionalString(chunk.GetValue(1, index));
    last_name = db_utils::toOptionalString(chunk.GetValue(2, index));
    additional_info = db_utils::toOptionalString(chunk.GetValue(3, index));
    diagnosis = db_utils::toOptionalString(chunk.GetValue(4, index));
    birthday_date =
        db_utils::toOptionalTimestampMs(chunk.GetValue(5, index));
    email = db_utils::toOptionalString(chunk.GetValue(6, index));
    phone_number = db_utils::toOptionalString(chunk.GetValue(7, index));
    client_active = db_utils::toBool(chunk.GetValue(8, index));
    country = db_utils::toOptionalString(chunk.GetValue(9, index));
    city = db_utils::toOptionalString(chunk.GetValue(10, index));
    time_zone = db_utils::toOptionalString(chunk.GetValue(11, index));
  }
};
inline std::ostream &operator<<(std::ostream &os, const DuckClient &c) {
  os << "DuckClient{"
     << "id=" << c.id << ", "
     << "name=";
  print_optional(os, c.name) << ", "
                             << "last_name=";
  print_optional(os, c.last_name) << ", "
                                  << "email=";
  print_optional(os, c.email)
      << ", "
      << "client_active=" << (c.client_active ? "true" : "false") << ", "
      << "birthday_date=";
  print_optional(os, c.birthday_date) << ", "
                                      << "country=";
  print_optional(os, c.country) << ", "
                                << "city=";
  print_optional(os, c.city) << ", "
                             << "time_zone=";
  print_optional(os, c.time_zone) << "}";
  return os;
}
// --- DuckEvent ---
struct DuckEvent {
  std::int64_t id = -1;
  std::optional<std::string> name = std::nullopt;
  std::optional<std::string> description = std::nullopt;
  std::optional<std::string> client_name = std::nullopt;
  bool is_work_event = false;
  std::int64_t event_stat_id = -1;
  std::int64_t payment_stat_id = -1;
  std::optional<std::int64_t> start_date = std::nullopt;
  std::optional<std::int64_t> end_date = std::nullopt;
  std::optional<std::int64_t> duration = std::nullopt;
  DuckEvent() = default;
  DuckEvent(const duckdb::DataChunk &chunk, duckdb::idx_t index) {
    id = db_utils::toInt32AsInt64(chunk.GetValue(0, index));
    name = db_utils::toOptionalString(chunk.GetValue(1, index));
    description = db_utils::toOptionalString(chunk.GetValue(2, index));
    is_work_event = db_utils::toBool(chunk.GetValue(3, index));
    event_stat_id = db_utils::toInt32AsInt64(chunk.GetValue(4, index));
    payment_stat_id = db_utils::toInt32AsInt64(chunk.GetValue(5, index));
    start_date = db_utils::toOptionalTimestampMs(chunk.GetValue(6, index));
    end_date = db_utils::toOptionalTimestampMs(chunk.GetValue(7, index));
    duration = db_utils::toOptionalInt32AsInt64(chunk.GetValue(8, index));
  }
};
inline std::ostream &operator<<(std::ostream &os, const DuckEvent &e) {
  os << "DuckEvent{"
     << "id=" << e.id << ", "
     << "name=";
  print_optional(os, e.name)
      << ", "
      << "is_work_event=" << (e.is_work_event ? "true" : "false") << ", "
      << "event_stat_id=" << e.event_stat_id << ", "
      << "payment_stat_id=" << e.payment_stat_id << ", "
      << "start_date=";
  print_optional(os, e.start_date) << ", "
                                   << "end_date=";
  print_optional(os, e.end_date) << ", "
                                 << "duration=";
  print_optional(os, e.duration) << "}";
  return os;
}
// --- DuckEventClient ---
struct DuckEventClient {
  std::int64_t id = -1;
  std::int64_t client_id = -1;
  std::int64_t event_id = -1;
  DuckEventClient() = default;
  DuckEventClient(const duckdb::DataChunk &chunk, duckdb::idx_t index) {
    id = db_utils::toInt32AsInt64(chunk.GetValue(0, index));
    client_id = db_utils::toInt32AsInt64(chunk.GetValue(1, index));
    event_id = db_utils::toInt32AsInt64(chunk.GetValue(2, index));
  }
};
inline std::ostream &operator<<(std::ostream &os, const DuckEventClient &ec) {
  os << "DuckEventClient{id=" << ec.id << ", client_id=" << ec.client_id
     << ", event_id=" << ec.event_id << "}";
  return os;
}
