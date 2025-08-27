#include <Poco/Timestamp.h>
#include <utility>
#include <vector>
#define OBX_CPP_FILE
#include "database.h"

namespace pcm::database {

Database::Database(const pcm::config::Config &conf) {
  plog::init(plog::verbose, "database.log");

  const auto db_pth = conf.db_conf.value_.db_pth;
  bool it_first_init = false;
  if (auto dir = Poco::File(db_pth); !dir.exists()) {
    dir.createDirectories();
    it_first_init = true;
  }

  auto options = obx::Options(create_obx_model());
  options.directory(db_pth.toString());

  m_store = std::make_unique<obx::Store>(options);
  m_events_box = std::make_unique<obx::Box<ObxEvent>>(*m_store);
  m_client_box = std::make_unique<obx::Box<ObxClient>>(*m_store);
  m_payment_status_box = std::make_unique<obx::Box<ObxPaymentStatus>>(*m_store);
  m_event_status_box = std::make_unique<obx::Box<ObxEventStatus>>(*m_store);
  m_event_client_box = std::make_unique<obx::Box<ObxEventClient>>(*m_store);

  if (it_first_init) {
    init_payment_status_table();
    init_event_status_table();
    add_demo_data();
  }
}

obx_id Database::add_event(const ObxEvent &event) {
  const obx_id id_inserted_item{m_events_box->put(event)};

  return id_inserted_item;
}

bool Database::remove_event(const obx_id &id) {
  const bool is_removed{m_events_box->remove(id)};
  return is_removed;
}

std::unique_ptr<ObxEvent> Database::get_event(const obx_id &id) {
  auto event = m_events_box->get(id);
  return std::move(event);
}

obx_id Database::add_client(const ObxClient &client) {
  const obx_id id{m_client_box->put(client)};
  return id;
}

bool Database::remove_client(const obx_id &id) {
  const bool is_removed{m_client_box->remove(id)};
  return is_removed;
}

obx_id Database::add_event_client(const obx_id &event_id,
                                const obx_id &client_id) {
  ObxEventClient obj = {.client_id = client_id, .event_id = event_id};
  m_event_client_box->put(obj);
}


std::unique_ptr<ObxClient> Database::get_client(const obx_id &id) {
  return m_client_box->get(id);
}

std::vector<std::unique_ptr<ObxClient>> Database::get_clients() {
  return m_client_box->getAll();
}

std::vector<obx_id> Database::get_client_ids() {
  const auto clients = m_client_box->getAll();
  const auto ids = clients | std::views::transform(
                                 [](const auto &client) { return client->id; });
  return std::vector<obx_id>{ids.begin(), ids.end()};
}

void Database::add_demo_data() {
  m_client_box->put(ObxClient{
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
  auto query_builder = m_events_box->query(ObxEvent_::start_date.equals(date));
  auto query = query_builder.build();
  std::vector<ObxEvent> results = query.find();
  const auto ids = results | std::views::transform(
                                 [](const auto &event) { return event.id; });
  return std::vector<obx_id>{ids.begin(), ids.end()};
}

bool Database::has_conflict(const ObxEvent &event) {
  auto query =
      m_events_box
          ->query(ObxEvent_::start_date.lessThan(event.end_date)
                      .and_(ObxEvent_::end_date.greaterThan(event.start_date)))
          .build();
  return query.count() > 0;
}

std::vector<ObxEvent> Database::get_day_events(const int64_t &date) {
  PLOGD << "Input date" << date;
  // Convert to microseconds.
  const auto [start, end] = get_time_range(Poco::Timestamp{date * 1000});
  PLOGD << "start:" << start << ", end:" << end;

  auto query = m_events_box
                   ->query(ObxEvent_::start_date.lessThan(end).and_(
                       ObxEvent_::end_date.greaterThan(start)))
                   .build();

  const auto result = query.find();
  return result;
}

} // namespace pcm::database
