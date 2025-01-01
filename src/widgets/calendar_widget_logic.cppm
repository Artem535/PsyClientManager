module;

#include <calendar.h>
#include <cassert>
#include <chrono>
#include <memory>
#include <ranges>
#include <slint.h>
#include <vector>

export module widgets.calendar;

namespace ch = std::chrono;

namespace pcm::widgets::calendar {

// Export classes created by slint
export using ::ICalendarLogic;
export using ::CalendarItem;
export using ::CalendarWeek;

constexpr CalendarItem
to_calendar_item(const ch::time_point<ch::system_clock> &time) {
  const ch::year_month_day current_date{ch::floor<ch::days>(time)};

  // Function that cast ch::day or ch::months to unsigned int.
  const auto to_int = [](const auto &value) {
    return static_cast<int>(static_cast<unsigned>(value));
  };

  return {
      .visible = true,
      .day = to_int(current_date.day()),
      .month = to_int(current_date.month()),
      .year = static_cast<int>(current_date.year()),
  };
}

constexpr ch::time_point<ch::system_clock>
to_std_time_point(const CalendarItem &value) {
  const ch::day day_std{static_cast<unsigned>(value.day)};
  const ch::month month_std{static_cast<unsigned>(value.month)};
  const ch::year year_std{value.year};

  const auto date = ch::year_month_day(year_std, month_std, day_std);
  assert(date.ok() && "Date is valid.");

  return ch::sys_days(date);
}

export inline CalendarItem get_current_date() {
  return to_calendar_item(ch::system_clock::now());
}

export inline std::shared_ptr<slint::VectorModel<CalendarWeek>>
get_calendar_matrix() {
  std::vector<CalendarWeek> month_data{5};
  for (auto &week : month_data) {
    const auto week_data = std::vector<CalendarItem>{7, {.day = -1}};
    week.week_data =
        std::make_shared<slint::VectorModel<CalendarItem>>(week_data);
  }

  return std::make_shared<slint::VectorModel<CalendarWeek>>(month_data);
}

/**
 * @brief Calculates the week number for a given day within a month.
 *
 * @param week_date_num The day of the week as an integer, where
 *                      Monday is 1 and Sunday is 7.
 * @param current_day The current day of the month. From 1 to 31.
 * @return The week number (0 to 5) where the given day falls.
 */
inline unsigned get_week_num(const int &week_date_num,
                             const uint &current_day) {
  return (current_day + week_date_num - 2) / 7;
}

export auto get_month_data(const int &year, const int &month) {
  auto month_data = get_calendar_matrix();

  const ch::year year_std{year};
  const ch::month month_std{static_cast<unsigned>(month)};

  const auto end_date = year_std / month_std / ch::last;
  const auto first_weekday = ch::weekday(year_std / month_std / ch::day{1});

  for (const uint day : std::views::iota(
           static_cast<uint>(1), static_cast<uint>(end_date.day()) + 1)) {
    const auto full_date = year_std / month_std / day;
    const auto week_date_num = ch::weekday(full_date).iso_encoding() - 1;
    const auto week_num = get_week_num(first_weekday.iso_encoding(), day);

    const auto month_row = month_data->row_data(week_num);
    month_row->week_data->set_row_data(
        week_date_num, to_calendar_item(ch::sys_days(full_date)));
  }

  return month_data;
};

export void connect_logic(const ICalendarLogic &item) {
  item.on_get_current_date(get_current_date);
  item.on_get_month_data(get_month_data);
}

} // namespace pcm::widgets::calendar