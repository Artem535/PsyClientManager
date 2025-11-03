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

  mDb = std::make_unique<duckdb::DuckDB>(db_pth.toString() + "/database.db");

  if (it_first_init) {
    init_tables();
    init_payment_status_table();
    init_event_status_table();
    add_demo_data();
  }
}

// --- Event ---

int64_t Database::add_event(const ObxEvent &event) {
  duckdb::Connection conn(*mDb);
  auto result =
      conn.Query(R"(
        INSERT INTO Event (
            name, description, is_work_event,
            event_stat_id, payment_stat_id,
            start_date, end_date, duration
        ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
        RETURNING id
    )",
                 db_utils::toDuckValue(event.name),
                 db_utils::toDuckValue(event.description), event.is_work_event,
                 event.event_stat_id, event.payment_stat_id,
                 db_utils::toDuckTimestamp(event.start_date.value_or(0) * 1000),
                 db_utils::toDuckTimestamp(event.end_date.value_or(0) * 1000),
                 db_utils::toDuckValue(event.duration));

  if (result->HasError()) {
    PLOG_ERROR << "Failed to insert event: " << result->GetError();
    return 0;
  }

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    PLOG_ERROR << "Empty result from RETURNING id in add_event";
    return 0;
  }

  return static_cast<int64_t>(chunk->GetValue(0, 0).GetValue<int32_t>());
}

bool Database::remove_event(const int64_t &id) {
  if (id <= 0) {
    PLOG_WARNING << "Attempt to remove event with invalid id: " << id;
    return false;
  }

  duckdb::Connection conn(*mDb);
  auto result = conn.Query("DELETE FROM Event WHERE id = $1", id);
  if (result->HasError()) {
    PLOG_ERROR << "Failed to delete event (id=" << id
               << "): " << result->GetError();
    return false;
  }
  PLOG_DEBUG << "Event deleted: id=" << id;
  return true;
}

std::unique_ptr<ObxEvent> Database::get_event(const int64_t &id) {
  if (id <= 0) {
    PLOG_WARNING << "Attempt to get event with invalid id: " << id;
    return nullptr;
  }

  duckdb::Connection conn(*mDb);
  auto result = conn.Query("SELECT * FROM Event WHERE id = $1", id);
  if (result->HasError()) {
    PLOG_ERROR << "Failed to query event (id=" << id
               << "): " << result->GetError();
    return nullptr;
  }

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    PLOG_DEBUG << "Event not found: id=" << id;
    return nullptr;
  }

  return std::make_unique<ObxEvent>(*chunk, 0);
}

// --- Client ---

int64_t Database::add_client(const ObxClient &client) {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(
      R"(
        INSERT INTO Client (
            name, last_name, additional_info, diagnosis,
            birthday_date, email, phone_number, client_active,
            country, city, time_zone
        ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
        RETURNING id
    )",
      db_utils::toDuckValue(client.name),
      db_utils::toDuckValue(client.last_name),
      db_utils::toDuckValue(client.additional_info),
      db_utils::toDuckValue(client.diagnosis),
      db_utils::toDuckTimestamp(client.birthday_date),
      db_utils::toDuckValue(client.email),
      db_utils::toDuckValue(client.phone_number), client.client_active,
      db_utils::toDuckValue(client.country), db_utils::toDuckValue(client.city),
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

  return static_cast<int64_t>(chunk->GetValue(0, 0).GetValue<int32_t>());
}

std::unique_ptr<ObxClient> Database::get_client(const int64_t &id) {
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

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    PLOG_DEBUG << "Client not found: id=" << id;
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

std::vector<int64_t> Database::get_client_ids() {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query("SELECT id FROM Client");
  if (result->HasError()) {
    PLOG_ERROR << "Failed to fetch client IDs: " << result->GetError();
    return {};
  }

  std::vector<int64_t> ids;
  while (auto chunk = result->Fetch()) {
    for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
      ids.push_back(
          static_cast<int64_t>(chunk->GetValue(0, i).GetValue<int32_t>()));
    }
  }
  return ids;
}

bool Database::remove_client(const int64_t &id) {
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

// --- EventClient ---

int64_t Database::add_event_client(const int64_t &event_id,
                                  const int64_t &client_id) {
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

  return static_cast<int64_t>(chunk->GetValue(0, 0).GetValue<int32_t>());
}

// --- Events by date / conflict ---

// std::vector<int64_t> Database::get_event_ids(const int64_t date_microseconds) {
//   // date_microseconds — уже в микросекундах (как у вас в ObxEvent)
//   duckdb::Connection conn(*mDb);
//
//   // Предположим, что вы хотите события, которые начинаются в этот день.
//   // Но лучше уточнить логику. Пока — точное совпадение по start_date.
//   auto result = conn.Query(
//       "SELECT id FROM Event",
//       db_utils::toDuckTimestamp(std::make_optional(date_microseconds)));
//
//   if (result->HasError()) {
//     PLOG_ERROR << "Failed to get event IDs: " << result->GetError();
//     return {};
//   }
//
//   std::vector<int64_t> ids;
//   while (auto chunk = result->Fetch()) {
//     for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
//       ids.push_back(
//           static_cast<int64_t>(chunk->GetValue(0, i).GetValue<int32_t>()));
//     }
//   }
//   return ids;
// }

bool Database::has_conflict(const ObxEvent &event) {
  if (event.start_date <= 0 || event.end_date <= 0) {
    return false;
  }

  duckdb::Connection conn(*mDb);
  auto result = conn.Query(R"(
        SELECT 1 FROM Event
        WHERE start_date < $1 AND end_date > $2
        LIMIT 1
    )",
                           db_utils::toDuckTimestamp(event.end_date),
                           db_utils::toDuckTimestamp(event.start_date));

  if (result->HasError()) {
    PLOG_ERROR << "Conflict check failed: " << result->GetError();
    return false;
  }

  // Если есть хотя бы одна строка — конфликт есть
  return result->Fetch() != nullptr;
}

std::vector<ObxEvent>
Database::get_day_events(const int64_t &date_ms) {
  const auto[start_day, end_day] = get_time_range(date_ms);

  duckdb::Connection conn(*mDb);
  auto result = conn.Query(
      R"(
        SELECT * FROM Event
        WHERE start_date <= $1 AND end_date >= $2
    )",
      db_utils::toDuckTimestamp(std::make_optional(end_day * 1000)),
      db_utils::toDuckTimestamp(std::make_optional(start_day * 1000)));

  if (result->HasError()) {
    PLOG_ERROR << "Failed to get day events: " << result->GetError();
    return {};
  }

  std::vector<ObxEvent> events;
  while (auto chunk = result->Fetch()) {
    for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
      events.emplace_back(*chunk, i);
    }
  }
  return events;
}

ObxClient Database::get_client_by_event(const int64_t &event_id) {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(R"(
        SELECT c.*
        FROM Client c
        JOIN EventClient ec ON c.id = ec.client_id
        WHERE ec.event_id = $1
    )",
                           event_id);

  if (result->HasError()) {
    PLOG_ERROR << "Failed to get client for event " << event_id << ": "
               << result->GetError();
    throw std::runtime_error("Client not found for event: " +
                             std::to_string(event_id));
  }

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    throw std::runtime_error("Client not found for event: " +
                             std::to_string(event_id));
  }

  return ObxClient(*chunk, 0);
}

// --- Init ---

void Database::add_demo_data() {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(constance::kDemoData);
  if (result->HasError()) {
    PLOG_ERROR << "Error inserting demo data: " << result->GetError();
  }
}

void Database::init_tables() {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(constance::kCreateTables);
  if (result->HasError()) {
    PLOG_ERROR << "Error creating tables: " << result->GetError();
  }
}

void Database::init_payment_status_table() {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(constance::kPaymentStatus);
  if (result->HasError()) {
    PLOG_ERROR << "Error inserting payment statuses: " << result->GetError();
  }
}

void Database::init_event_status_table() {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(constance::kEventStatus);
  if (result->HasError()) {
    PLOG_ERROR << "Error inserting event statuses: " << result->GetError();
  }
}

} // namespace pcm::database
