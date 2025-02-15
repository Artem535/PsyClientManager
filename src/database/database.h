// app_database.h
#pragma once
#include <memory>
#include <vector>
#include <iostream>

#include "objectbox.hpp"
#include "objectbox-model.h"
#include "scheme.obx.hpp"

#include "config.h"
#include "constance.hpp"
#include <Poco/File.h>

namespace pcm::database {

class Database {
public:
    explicit Database(const pcm::config::Config &conf);
    obx_id add_event(const Events &event);
    bool remove_event(const obx_id &id);
    obx_id add_client(const Client &client);
    bool remove_client(const obx_id &id);
    std::unique_ptr<Client> get_client(const obx_id &id);
    std::vector<std::unique_ptr<Client>> get_clients();

private:
    void add_demo_data();
    void init_payment_status_table();
    void init_event_status_table();

    std::unique_ptr<obx::Store> m_store;
    std::unique_ptr<obx::Box<Events>> m_events_box;
    std::unique_ptr<obx::Box<Client>> m_clinet_box;
    std::unique_ptr<obx::Box<PaymentStatus>> m_payment_status_box;
    std::unique_ptr<obx::Box<EventStatus>> m_event_status_box;
};

} // namespace pcm::database