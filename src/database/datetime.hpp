#pragma once

#include <Poco/DateTime.h>
#include <Poco/Timestamp.h>
#include <plog/Log.h>
#include <utility>

inline std::pair<int64_t, int64_t>
get_time_range(const Poco::Timestamp &timestamp) {
  const auto input_date = Poco::DateTime{timestamp};
  PLOGD << "Input date: " << input_date.year() << '-' << input_date.month()
        << '-' << input_date.day();

  const auto start_of_day = Poco::DateTime{
      input_date.year(), input_date.month(), input_date.day(), 0, 0, 0, 0, 0};

  const auto start_micros = start_of_day.timestamp().epochMicroseconds();
  constexpr int64_t MICROS_PER_DAY = 24LL * 60 * 60 * 1'000'000;
  const auto end_micros = start_micros + MICROS_PER_DAY - 1;

  return {start_micros, end_micros};
}