#define OBX_CPP_FILE
#include "database.h"

namespace pcm::database {

Database::Database(const pcm::config::Config &conf) {
    auto db_pth = conf.db_conf.value_.db_pth;
    bool it_first_init = false;
    if (auto dir = Poco::File(db_pth); !dir.exists()) {
        dir.createDirectories();
        it_first_init = true;
    }

    auto options = obx::Options(create_obx_model());
    options.directory(db_pth.toString());
    
    m_store = std::make_unique<obx::Store>(options);
    m_events_box = std::make_unique<obx::Box<Events>>(*m_store.get());
    m_clinet_box = std::make_unique<obx::Box<Client>>(*m_store.get());
    m_payment_status_box = std::make_unique<obx::Box<PaymentStatus>>(*m_store.get());
    m_event_status_box = std::make_unique<obx::Box<EventStatus>>(*m_store.get());

    if (it_first_init) {
        init_payment_status_table();
        init_event_status_table();
        add_demo_data();
    }
}

obx_id Database::add_event(const Events &event) {
    return m_events_box->put(event);
}

bool Database::remove_event(const obx_id &id) {
    return m_events_box->remove(id);
}

obx_id Database::add_client(const Client &client) {
    return m_clinet_box->put(client);
}

bool Database::remove_client(const obx_id &id) {
    return m_clinet_box->remove(id);
}

std::unique_ptr<Client> Database::get_client(const obx_id &id) {
    return m_clinet_box->get(id);
}

std::vector<std::unique_ptr<Client>> Database::get_clients() {
    return m_clinet_box->getAll();
}

void Database::add_demo_data() {
    const auto id = m_clinet_box->put(Client{.name = "John", .last_name = "Doe", .additional_info = "Some add info"});
    std::cout << id << std::endl;
}

void Database::init_payment_status_table() {
    for (const auto &status : constance::payment_statuses)
        m_payment_status_box->put(status);
}

void Database::init_event_status_table() {
    for (const auto &status : constance::event_statuses)
        m_event_status_box->put(status);
}

} // namespace pcm::database
