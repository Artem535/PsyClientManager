#include "database.h"

namespace pcm::database {
namespace {
duckdb::Value fkOrNull(const int64_t id) {
  return id > 0 ? duckdb::Value::INTEGER(static_cast<int32_t>(id))
                : duckdb::Value();
}

int32_t statusOrDefault(const int64_t id) {
  return static_cast<int32_t>(id > 0 ? id : 1);
}
} // namespace

Database::Database(const config::Config &conf) {
  const auto db_pth = conf.db_conf.value_.db_pth;

  plog::init(plog::verbose, (db_pth.toString() + "/database.log").c_str());

  bool it_first_init = false;
  if (auto dir = Poco::File(db_pth); !dir.exists()) {
    dir.createDirectories();
    it_first_init = true;
  }

  mDb = std::make_unique<duckdb::DuckDB>(db_pth.toString() + "/database.db");

  init_tables();
  apply_schema_migrations();
  init_payment_status_table();
  init_event_status_table();

  if (it_first_init) {
    add_demo_data();
  }
}

// --- Event ---

int64_t Database::add_event(const DuckEvent &event, const bool allowOverlap) {
  PLOG_DEBUG << "add_event request: start_ms="
             << event.start_date.value_or(-1)
             << ", end_ms=" << event.end_date.value_or(-1)
             << ", event_stat_id=" << event.event_stat_id
             << ", payment_stat_id=" << event.payment_stat_id;

  if (!allowOverlap && has_conflict(event)) {
    PLOG_WARNING << "Rejected event insert because of time conflict";
    return 0;
  }

  duckdb::Connection conn(*mDb);
  auto result =
      conn.Query(R"(
        INSERT INTO Event (
            id,
            name, description, is_work_event,
            event_stat_id, payment_stat_id,
            start_date, end_date, duration, cost
        )
        SELECT
            COALESCE(MAX(id), 0) + 1,
            $1, $2, $3, $4, $5, $6, $7, $8, $9
        FROM Event
        RETURNING id
    )",
                 db_utils::toDuckValue(event.name),
                 db_utils::toDuckValue(event.description), event.is_work_event,
                 statusOrDefault(event.event_stat_id),
                 statusOrDefault(event.payment_stat_id),
                 db_utils::toDuckTimestamp(event.start_date.value_or(0) * 1000),
                 db_utils::toDuckTimestamp(event.end_date.value_or(0) * 1000),
                 db_utils::toDuckValue(event.duration),
                 db_utils::toDuckValue(event.cost));

  if (result->HasError()) {
    PLOG_ERROR << "Failed to insert event: " << result->GetError();
    return 0;
  }

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    PLOG_ERROR << "Empty result from RETURNING id in add_event";
    return 0;
  }

  const auto newId =
      static_cast<int64_t>(chunk->GetValue(0, 0).GetValue<int32_t>());
  PLOG_DEBUG << "Inserted event id=" << newId;
  return newId;
}

bool Database::update_event(const DuckEvent &event, const bool allowOverlap) {
  if (event.id <= 0) {
    PLOG_WARNING << "Attempt to update event with invalid id: " << event.id;
    return false;
  }

  if (!allowOverlap && has_conflict(event)) {
    PLOG_WARNING << "Rejected event update because of time conflict for id="
                 << event.id;
    return false;
  }

  duckdb::Connection conn(*mDb);
  auto result = conn.Query(R"(
        UPDATE Event
        SET name = $1,
            description = $2,
            is_work_event = $3,
            event_stat_id = COALESCE($4, event_stat_id),
            payment_stat_id = COALESCE($5, payment_stat_id),
            start_date = $6,
            end_date = $7,
            duration = $8,
            cost = $9
        WHERE id = $10
    )",
                           db_utils::toDuckValue(event.name),
                           db_utils::toDuckValue(event.description),
                           event.is_work_event, fkOrNull(event.event_stat_id),
                           fkOrNull(event.payment_stat_id),
                           db_utils::toDuckTimestamp(event.start_date.value_or(0) * 1000),
                           db_utils::toDuckTimestamp(event.end_date.value_or(0) * 1000),
                           db_utils::toDuckValue(event.duration),
                           db_utils::toDuckValue(event.cost), event.id);

  if (result->HasError()) {
    PLOG_ERROR << "Failed to update event (id=" << event.id
               << "): " << result->GetError();
    return false;
  }

  return true;
}

bool Database::remove_event(const int64_t &id) {
  if (id <= 0) {
    PLOG_WARNING << "Attempt to remove event with invalid id: " << id;
    return false;
  }

  duckdb::Connection conn(*mDb);
  auto relationResult =
      conn.Query("DELETE FROM EventClient WHERE event_id = $1", id);
  if (relationResult->HasError()) {
    PLOG_ERROR << "Failed to delete EventClient links for event (id=" << id
               << "): " << relationResult->GetError();
    return false;
  }

  auto result = conn.Query("DELETE FROM Event WHERE id = $1", id);
  if (result->HasError()) {
    PLOG_ERROR << "Failed to delete event (id=" << id
               << "): " << result->GetError();
    return false;
  }
  PLOG_DEBUG << "Event deleted: id=" << id;
  return true;
}

std::unique_ptr<DuckEvent> Database::get_event(const int64_t &id) {
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

  return std::make_unique<DuckEvent>(*chunk, 0);
}

// --- Client ---

int64_t Database::add_client(const DuckClient &client) {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(
      R"(
        INSERT INTO Client (
            id,
            name, last_name, additional_info, diagnosis,
            birthday_date, email, phone_number, client_active,
            country, city, time_zone
        )
        SELECT
            COALESCE(MAX(id), 0) + 1,
            $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11
        FROM Client
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

std::unique_ptr<DuckClient> Database::get_client(const int64_t &id) {
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

  return std::make_unique<DuckClient>(*chunk, 0);
}

std::vector<std::unique_ptr<DuckClient>> Database::get_clients() {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query("SELECT * FROM Client");
  if (result->HasError()) {
    PLOG_ERROR << "Failed to fetch all clients: " << result->GetError();
    return {};
  }

  std::vector<std::unique_ptr<DuckClient>> clients;
  while (auto chunk = result->Fetch()) {
    for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
      clients.emplace_back(std::make_unique<DuckClient>(*chunk, i));
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
  if (event_id <= 0) {
    PLOG_WARNING << "Invalid event_id for EventClient: " << event_id;
    return 0;
  }

  duckdb::Connection conn(*mDb);
  auto removeResult = conn.Query("DELETE FROM EventClient WHERE event_id = $1",
                                 event_id);
  if (removeResult->HasError()) {
    PLOG_ERROR << "Failed to clear EventClient links for event_id=" << event_id
               << ": " << removeResult->GetError();
    return 0;
  }

  if (client_id <= 0) {
    return 0;
  }

  auto result = conn.Query(R"(
        INSERT INTO EventClient (id, client_id, event_id)
        SELECT COALESCE(MAX(id), 0) + 1, $1, $2
        FROM EventClient
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
    PLOG_ERROR << "Empty result from RETURNING id in add_event_client";
    return 0;
  }

  return static_cast<int64_t>(chunk->GetValue(0, 0).GetValue<int32_t>());
}

// --- Events by date / conflict ---

// std::vector<int64_t> Database::get_event_ids(const int64_t date_microseconds) {
//   // date_microseconds is already in microseconds (as in DuckEvent)
//   duckdb::Connection conn(*mDb);
//
//   // Assume we need events that start on this day.
//   // The exact rule should be clarified. For now: exact match by start_date.
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

bool Database::has_conflict(const DuckEvent &event) {
  if (!event.start_date.has_value() || !event.end_date.has_value()) {
    return false;
  }

  duckdb::Connection conn(*mDb);
  auto result = conn.Query(R"(
        SELECT 1 FROM Event
        WHERE id != $1
          AND start_date < $2
          AND end_date > $3
        LIMIT 1
    )",
                           event.id,
                           db_utils::toDuckTimestamp(event.end_date.value() * 1000),
                           db_utils::toDuckTimestamp(event.start_date.value() * 1000));

  if (result->HasError()) {
    PLOG_ERROR << "Conflict check failed: " << result->GetError();
    return false;
  }

  // If there is at least one row, a conflict exists.
  return result->Fetch() != nullptr;
}

std::vector<DuckEvent>
Database::get_day_events(const int64_t &start_ms, const int64_t &end_ms) {
  const auto start_day = start_ms * 1000;
  const auto end_day = end_ms * 1000;
  PLOG_DEBUG << "get_day_events for range_ms=[" << start_ms << ", " << end_ms
             << "] range_micros=[" << start_day << ", " << end_day << "]";

  duckdb::Connection conn(*mDb);
  auto result = conn.Query(
      R"(
        SELECT * FROM Event
        WHERE start_date <= $1 AND end_date >= $2
    )",
      db_utils::toDuckTimestamp(std::make_optional(end_day)),
      db_utils::toDuckTimestamp(std::make_optional(start_day)));

  if (result->HasError()) {
    PLOG_ERROR << "Failed to get day events: " << result->GetError();
    return {};
  }

  std::vector<DuckEvent> events;
  while (auto chunk = result->Fetch()) {
    for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
      events.emplace_back(*chunk, i);
      const auto &ev = events.back();
      PLOG_DEBUG << "get_day_events row id=" << ev.id
                 << " start_ms=" << ev.start_date.value_or(-1)
                 << " end_ms=" << ev.end_date.value_or(-1);
    }
  }
  PLOG_DEBUG << "get_day_events loaded count=" << events.size();
  return events;
}

DuckClient Database::get_client_by_event(const int64_t &event_id) {
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

  return DuckClient(*chunk, 0);
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

void Database::apply_schema_migrations() {
  duckdb::Connection conn(*mDb);
  auto result = conn.Query(constance::kSchemaMigrations);
  if (result->HasError()) {
    PLOG_ERROR << "Error applying schema migrations: " << result->GetError();
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
