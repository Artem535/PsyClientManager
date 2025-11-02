#define OBX_CPP_FILE
#include "database.h"

namespace pcm::database {

Database::Database(const config::Config &conf) {
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

obx_id Database::add_event(const ObxEvent &event) const {
  auto connection = duckdb::Connection(*mDb);
  {
    const auto prepare = connection.Prepare(
        "INSERT INTO Event (name, description, is_work_event, "
        "event_stat_id, payment_stat_id, start_date, end_date, duration) "
        "VALUES "
        "($1, $2, $3, $4, $5, $6, $7, $8)");
    const auto result = prepare->Execute(
        db_utils::toDuckValue(event.name),
        db_utils::toDuckValue(event.description), event.is_work_event,
        event.event_stat_id, event.payment_stat_id,
        db_utils::toDuckTimestamp(event.start_date),
        db_utils::toDuckTimestamp(event.end_date),
        db_utils::toDuckValue(event.duration));

    if (result->HasError()) {
      PLOGE << "Database error: " << result->GetError();
    } else {
      PLOGD << "Inserted to database, event:" << event;
    }
  }

  int64_t id = -1;
  if (const auto result = connection.Query("SELECT last_insert_rowid()");
      !result->HasError()) {
    const auto row = result->Fetch();
    const auto value = row->GetValue(0, 0);
    id = value.GetValue<int64_t>();
  }

  return id;
}

bool Database::remove_event(const int64_t &id) {
  auto connection = duckdb::Connection(*mDb);
  const auto prepare = connection.Prepare("DELETE FROM Event WHERE id = $1");
  const auto result = prepare->Execute(id);

  bool is_removed = true;
  if (result->HasError()) {
    PLOGE << "Database error: " << result->GetError();
    is_removed = false;
  }

  return is_removed;
}

std::unique_ptr<ObxEvent> Database::get_event(const obx_id &id) const {
  auto connection = duckdb::Connection(*mDb);
  const auto select = "SELECT id, name, description, is_work_event, "
                      "event_stat_id, payment_stat_id, start_date, end_date, "
                      "duration "
                      "FROM Event "
                      "WHERE id = $1;";
  const auto prepare = connection.Prepare(select);

  if (const auto result = prepare->Execute(id); result->HasError()) {
    PLOGE << "Database error: " << result->GetError();
  } else {
    PLOGD << "Selected event id: " << id;
    const auto chunk = result->Fetch();
    constexpr int row_idx = 0;
    return std::make_unique<ObxEvent>(*chunk, row_idx);
  }

  return nullptr;
}

obx_id Database::add_client(const ObxClient &client) {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(R"(
        INSERT INTO Client (
            name, last_name, additional_info, diagnosis,
            birthday_date, email, phone_number, client_active,
            country, city, time_zone
        ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
        RETURNING id)",
                           db_utils::toDuckValue(client.name),
                           db_utils::toDuckValue(client.last_name),
                           db_utils::toDuckValue(client.additional_info),
                           db_utils::toDuckValue(client.diagnosis),
                           db_utils::toDuckTimestamp(client.birthday_date),
                           db_utils::toDuckValue(client.email),
                           db_utils::toDuckValue(client.phone_number),
                           db_utils::toDuckValue(client.client_active),
                           db_utils::toDuckValue(client.country),
                           db_utils::toDuckValue(client.city),
                           db_utils::toDuckValue(client.time_zone));

  if (result->HasError()) {
    PLOG_ERROR << "Failed to insert client: " << result->GetError();
    return 0;
  }

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    PLOG_ERROR << "Empty result from RETURNING id in add_client";
    return 0;
  }

  return static_cast<obx_id>(chunk->GetValue(0, 0).GetValue<int32_t>());
}

std::unique_ptr<ObxClient> Database::get_client(const obx_id &id) {
  if (id <= 0) {
    PLOG_WARNING << "Attempt to get client with invalid id: " << id;
    return nullptr;
  }

  duckdb::Connection conn(*mDb);
  auto result = conn.Query("SELECT * FROM Client WHERE id = $1", id);
  if (result->HasError()) {
    PLOG_ERROR << "Failed to query client (id=" << id
               << "): " << result->GetError();
    return nullptr;
  }
  if (result->Fetch() == nullptr) {
    PLOG_DEBUG << "Client not found: id=" << id;
    return nullptr;
  }

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    PLOG_ERROR << "Unexpected empty chunk for client id=" << id;
    return nullptr;
  }

  return std::make_unique<ObxClient>(*chunk, 0);
}

std::vector<std::unique_ptr<ObxClient>> Database::get_clients() {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query("SELECT * FROM Client");
  if (result->HasError()) {
    PLOG_ERROR << "Failed to fetch all clients: " << result->GetError();
    return {};
  }

  std::vector<std::unique_ptr<ObxClient>> clients;
  while (auto chunk = result->Fetch()) {
    for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
      clients.emplace_back(std::make_unique<ObxClient>(*chunk, i));
    }
  }
  PLOG_DEBUG << "Loaded " << clients.size() << " clients";
  return clients;
}

bool Database::remove_client(const obx_id &id) {
  if (id <= 0) {
    PLOG_WARNING << "Attempt to remove client with invalid id: " << id;
    return false;
  }

  duckdb::Connection conn(*mDb);
  auto result = conn.Query("DELETE FROM Client WHERE id = $1", id);
  if (result->HasError()) {
    PLOG_ERROR << "Failed to delete client (id=" << id
               << "): " << result->GetError();
    return false;
  }
  PLOG_DEBUG << "Client deleted: id=" << id;
  return true;
}

obx_id Database::add_event_client(const obx_id &event_id,
                                  const obx_id &client_id) {
  if (event_id <= 0 || client_id <= 0) {
    PLOG_WARNING << "Invalid IDs for EventClient: event_id=" << event_id
                 << ", client_id=" << client_id;
    return 0;
  }

  duckdb::Connection conn(*mDb);
  auto result = conn.Query(R"(
        INSERT INTO EventClient (client_id, event_id)
        VALUES ($1, $2)
        ON CONFLICT (client_id, event_id) DO NOTHING
        RETURNING id
    )",
                           client_id, event_id);

  if (result->HasError()) {
    PLOG_ERROR << "Failed to link event_id=" << event_id
               << " and client_id=" << client_id << ": " << result->GetError();
    return 0;
  }

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    PLOG_DEBUG << "EventClient link already exists: event=" << event_id
               << ", client=" << client_id;
    return 0;
  }

  return static_cast<obx_id>(chunk->GetValue(0, 0).GetValue<int32_t>());
}
std::vector<obx_id> Database::get_client_ids() {
  const auto clients = m_client_box->getAll();
  const auto ids = clients | std::views::transform(
                                 [](const auto &client) { return client->id; });
  return std::vector<obx_id>{ids.begin(), ids.end()};
}

void Database::add_demo_data() {
  auto connection = duckdb::Connection(*mDb);
  if (const auto result = connection.Query(constance::kDemoData);
      result->HasError()) {
    PLOGE << "Error| Create tables: " << result->GetError();
  }
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
