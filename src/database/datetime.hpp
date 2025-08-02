#include <Poco/DateTime.h>
#include <Poco/Timestamp.h>
#include <utility>

inline std::pair<int64_t, int64_t>
get_time_range(const Poco::Timestamp &timestamp) {
  auto input_date = Poco::DateTime{timestamp};

  const auto start_of_day = Poco::DateTime{
      input_date.year(), input_date.month(), input_date.day(), 0, 0};

  const auto end_of_day = Poco::DateTime{
      input_date.year(), input_date.month(), input_date.day(), 23, 59, 59};

  return {start_of_day.timestamp().epochTime(),
          end_of_day.timestamp().epochTime()};
}