#define OBX_CPP_FILE
#include "database.h"

namespace pcm::database {

Database::Database(const pcm::config::Config &conf) {
  const auto db_pth = conf.db_conf.value_.db_pth;

  plog::init(plog::verbose, (db_pth.toString() + "/database.log").c_str());

  bool it_first_init = false;
  if (auto dir = Poco::File(db_pth); !dir.exists()) {
    dir.createDirectories();
    it_first_init = true;
  }

  auto options = obx::Options(create_obx_model());
  options.directory(db_pth.toString());

  mDb = std::make_unique<duckdb::DuckDB>(db_pth.toString() + "/database.db");

  if (it_first_init) {
    init_tables();
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
  const auto id = m_event_client_box->put(obj);
  return id;
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
void Database::init_tables() {
  auto connection = duckdb::Connection(*mDb);
  if (const auto result = connection.Query(constance::kCreateTables);
      result->HasError()) {
    PLOGE << "Error| Create tables: " << result->GetError();
  }
}

void Database::init_payment_status_table() {
  auto connection = duckdb::Connection(*mDb);
  if (const auto result = connection.Query(constance::kPaymentStatus);
      result->HasError()) {
    PLOGE << "Error| Insert payment status: " << result->GetError();
  }
}

void Database::init_event_status_table() {
  auto connection = duckdb::Connection(*mDb);
  if (const auto result = connection.Query(constance::kEventStatus);
      result->HasError()) {
    PLOGE << "Error| Insert event status: " << result->GetError();
  }
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

ObxClient Database::get_client_by_event(const obx_id &event_id) {
  // TODO: Move to relations instead of query.
  // clang-format off
  auto query = m_event_client_box->query(
  ObxEventClient_::event_id.equals(event_id)
  );
  // clang-format on
  auto build_query = query.build();
  const auto result = build_query.find();

  // TODO: Made it more safe.
  if (result.empty()) {
    throw std::runtime_error("Can't find client for event with id: " +
                             std::to_string(event_id));
  }

  const auto client_id = result[0].client_id;
  const auto client = m_client_box->get(client_id);

  return *client;
}

} // namespace pcm::database
