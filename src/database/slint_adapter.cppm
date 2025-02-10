module;
#include "client_info.h"
#include "slint_string.h"
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <objectbox.hpp>
#include <string>

export module slint_database_adapter;

import app_database;

namespace ch = std::chrono;

export namespace pcm::database::adapters {

using ::IClientInfoLogic;

class SlintDbAdapter {

public:
  explicit SlintDbAdapter(std::shared_ptr<database::Database> db) : m_db(db) {}

  void connect_logic(const IClientInfoLogic &item) {
    item.on_get_user_info(
        [this](slint::SharedString id) { return this->get_user_info(id); });
  }

  ClientInfoSlint get_user_info(slint::SharedString id) {
    const auto std_string = std::string(id.data());
    return std_string.empty() ? ClientInfoSlint()
                              : get_user_info(stol(std_string));
  }

  ClientInfoSlint get_user_info(obx_id id) {
    std::cout << id << std::endl;
    const auto client = m_db->get_client(id);
    return {.name = slint::SharedString(client->name),
            .last_name = slint::SharedString(client->last_name),
            .additional_info = slint::SharedString(client->additional_info),
            .age = client->age,
            .birthday_date =
                slint::SharedString(date_to_string(client->birthday_date))};
  }

private:
  [[nodiscard]] std::string date_to_string(int64_t date) {
    const auto clock = ch::system_clock::from_time_t(date);
    return std::format("{:%D}", clock);
  }

private:
  std::shared_ptr<database::Database> m_db;
};

} // namespace pcm::database::adapters