#include <utility>
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
  m_events_box = std::make_unique<obx::Box<Event>>(*m_store.get());
  m_clinet_box = std::make_unique<obx::Box<Client>>(*m_store.get());
  m_payment_status_box =
      std::make_unique<obx::Box<PaymentStatus>>(*m_store.get());
  m_event_status_box = std::make_unique<obx::Box<EventStatus>>(*m_store.get());

  if (it_first_init) {
    init_payment_status_table();
    init_event_status_table();
    add_demo_data();
  }
}

obx_id Database::add_event(const Event &event) {
  const obx_id id_inserted_item{m_events_box->put(event)};
  return id_inserted_item;
}

bool Database::remove_event(const obx_id &id) {
  const bool is_removed{m_events_box->remove(id)};
  return is_removed;
}

std::unique_ptr<Event> Database::get_event(const obx_id &id) {
  auto event = m_events_box->get(id);
  return std::move(event);
}

obx_id Database::add_client(const Client &client) {
  const obx_id id{m_clinet_box->put(client)};
  return id;
}

bool Database::remove_client(const obx_id &id) {
  const bool is_removed{m_clinet_box->remove(id)};
  return is_removed;
}

std::unique_ptr<Client> Database::get_client(const obx_id &id) {
  return m_clinet_box->get(id);
}

std::vector<std::unique_ptr<Client>> Database::get_clients() {
  return m_clinet_box->getAll();
}

std::vector<obx_id> Database::get_client_ids() {
  const auto clients = m_clinet_box->getAll();
  const auto ids = clients | std::views::transform(
                                 [](const auto &client) { return client->id; });
  return std::vector<obx_id>{ids.begin(), ids.end()};
}

void Database::add_demo_data() {
  m_clinet_box->put(Client{
      .name = "John", .last_name = "Doe", .additional_info = "Some add info"});
}

void Database::init_payment_status_table() {
  for (const auto &status : constance::payment_statuses)
    m_payment_status_box->put(status);
}

void Database::init_event_status_table() {
  for (const auto &status : constance::event_statuses)
    m_event_status_box->put(status);
}

std::vector<obx_id> Database::get_event_ids(const int64_t date) {
  auto query_builder = m_events_box->query(Event_::start_date.equals(date));
  auto query = query_builder.build();
  std::vector<Event> results = query.find();
  const auto ids = results | std::views::transform(
                                 [](const auto &event) { return event.id; });
  return std::vector<obx_id>{ids.begin(), ids.end()};
}

} // namespace pcm::database
