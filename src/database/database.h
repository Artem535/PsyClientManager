// app_database.h
#pragma once
#include <cstdint>
#include <memory>
#include <objectbox.h>
#include <vector>
#include <ranges>
#include <chrono>


#include "objectbox.hpp"
#include "objectbox-model.h"
#include "scheme.obx.hpp"
#include "datetime.hpp"

#include "config.h"
#include "constance.hpp"
#include <Poco/File.h>
#include <Poco/Timestamp.h>

namespace pcm::database {

class Database {
public:
    explicit Database(const pcm::config::Config &conf);
    
    obx_id add_event(const Event &event);
    std::unique_ptr<Event> get_event(const obx_id &id);
    bool remove_event(const obx_id &id);
    
    obx_id add_client(const Client &client);
    bool remove_client(const obx_id &id);

    std::unique_ptr<Client> get_client(const obx_id &id);
    std::vector<std::unique_ptr<Client>> get_clients();

    std::vector<obx_id> get_client_ids();
    std::vector<obx_id> get_event_ids(const int64_t date);

    bool has_conflict(const Event &event);
    std::vector<Event> get_day_events(const int64_t date); 



private:
    void add_demo_data();
    void init_payment_status_table();
    void init_event_status_table();

    std::unique_ptr<obx::Store> m_store;
    std::unique_ptr<obx::Box<Event>> m_events_box;
    std::unique_ptr<obx::Box<Client>> m_clinet_box;
    std::unique_ptr<obx::Box<PaymentStatus>> m_payment_status_box;
    std::unique_ptr<obx::Box<EventStatus>> m_event_status_box;
};

} // namespace pcm::database