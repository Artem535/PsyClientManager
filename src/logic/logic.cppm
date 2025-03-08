module;

#include "logic.h"
#include <memory>

export module logic;

import slint_database_adapter;
import widgets.calendar;

export namespace pcm::logic {
using ::ICalendarLogic;
using ::IClientInfoLogic;

void connect_db(const IClientInfoLogic &logic,
                const database::adapters::SlintDbAdapter &db) {

  logic.on_get_user_info([db](slint::SharedString id) {
    const auto client_info = db.get_user_info(id);
    return client_info;
  });
}

void connect_calendar(const ICalendarLogic &logic) {
  std::cout << "Hello from connect_calendar";
}

} // namespace pcm::logic