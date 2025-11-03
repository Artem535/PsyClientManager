// db_structs.hpp

#pragma once

#include "duckdb.hpp"
#include <optional>
#include <string>

namespace pcm::database::db_utils {


inline std::optional<std::string> toOptionalString(const duckdb::Value &val) {
  if (val.IsNull()) return std::nullopt;
  return val.ToString();
}

inline std::optional<std::int64_t> toOptionalInt32AsInt64(const duckdb::Value &val) {
  if (val.IsNull()) return std::nullopt;
  return val.GetValue<int32_t>();
}

inline std::int64_t toInt32AsInt64(const duckdb::Value &val) {
  return val.GetValue<int32_t>();
}

inline bool toBool(const duckdb::Value &val) {
  return val.GetValue<bool>();
}

inline std::optional<std::int64_t> toOptionalTimestampMicros(const duckdb::Value &val) {
  if (val.IsNull()) return std::nullopt;
  auto ts = val.GetValue<duckdb::timestamp_t>();
  return duckdb::Timestamp::GetEpochMicroSeconds(ts);
}

inline std::optional<std::int64_t> toOptionalTimestampMs(const duckdb::Value &val) {
  if (val.IsNull()) return std::nullopt;
  auto ts = val.GetValue<duckdb::timestamp_t>();
  return duckdb::Timestamp::GetEpochMicroSeconds(ts) / 1000;
}

inline duckdb::Value toDuckValue(const std::optional<std::string>& opt) {
  return opt.has_value() ? duckdb::Value(*opt) : duckdb::Value();
}

inline duckdb::Value toDuckValue(const std::optional<std::int64_t>& opt) {
  return opt.has_value() ? duckdb::Value(*opt) : duckdb::Value();
}

inline duckdb::Value toDuckTimestamp(const std::optional<std::int64_t>& micros_opt) {
  if (!micros_opt.has_value()) return duckdb::Value(); // NULL
  auto ts = duckdb::Timestamp::FromEpochMicroSeconds(*micros_opt);
  return duckdb::Value::TIMESTAMP(ts);
}

} // namespace pcm::database::db_utils