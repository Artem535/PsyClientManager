// constance.hpp

#pragma once

namespace pcm::database::constance {

constexpr auto kCreateTables = R"duckdb(
-- Payment statuses
CREATE TABLE IF NOT EXISTS PaymentStatus (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL
);

-- Event statuses
CREATE TABLE IF NOT EXISTS EventStatus (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL
);

-- Clients
CREATE TABLE IF NOT EXISTS Client (
    id INTEGER PRIMARY KEY,
    name TEXT,
    last_name TEXT,
    additional_info TEXT,
    diagnosis TEXT,
    birthday_date TIMESTAMP,
    email TEXT,
    phone_number TEXT,
    client_active BOOLEAN,
    country TEXT,
    city TEXT,
    time_zone TEXT
);

-- Events
CREATE TABLE IF NOT EXISTS Event (
    id INTEGER PRIMARY KEY,
    name TEXT,
    description TEXT,
    is_work_event BOOLEAN,
    event_stat_id INTEGER REFERENCES EventStatus(id),
    payment_stat_id INTEGER REFERENCES PaymentStatus(id),
    start_date TIMESTAMP,
    end_date TIMESTAMP,
    duration INTEGER,
    cost DOUBLE
);

-- Many-to-many relationship between clients and events
CREATE TABLE IF NOT EXISTS EventClient (
    id INTEGER PRIMARY KEY,
    client_id INTEGER NOT NULL REFERENCES Client(id),
    event_id INTEGER NOT NULL REFERENCES Event(id),
    UNIQUE (client_id, event_id)
);
)duckdb";

constexpr auto kSchemaMigrations = R"duckdb(
ALTER TABLE Event ADD COLUMN IF NOT EXISTS cost DOUBLE;
)duckdb";

constexpr auto kInsertEventQuery = R"duckdb(
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
)duckdb";

constexpr auto kUpdateEventQuery = R"duckdb(
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
)duckdb";

constexpr auto kDeleteEventClientByEventIdQuery =
    "DELETE FROM EventClient WHERE event_id = $1";
constexpr auto kDeleteEventByIdQuery = "DELETE FROM Event WHERE id = $1";
constexpr auto kSelectEventByIdQuery = "SELECT * FROM Event WHERE id = $1";

constexpr auto kInsertClientQuery = R"duckdb(
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
)duckdb";

constexpr auto kSelectClientByIdQuery = "SELECT * FROM Client WHERE id = $1";
constexpr auto kSelectAllClientsQuery = "SELECT * FROM Client";
constexpr auto kSelectAllClientIdsQuery = "SELECT id FROM Client";
constexpr auto kDeleteClientByIdQuery = "DELETE FROM Client WHERE id = $1";

constexpr auto kInsertEventClientQuery = R"duckdb(
INSERT INTO EventClient (id, client_id, event_id)
SELECT COALESCE(MAX(id), 0) + 1, $1, $2
FROM EventClient
RETURNING id
)duckdb";

constexpr auto kHasConflictQuery = R"duckdb(
SELECT 1 FROM Event
WHERE id != $1
  AND start_date < $2
  AND end_date > $3
LIMIT 1
)duckdb";

constexpr auto kSelectDayEventsQuery = R"duckdb(
SELECT * FROM Event
WHERE start_date <= $1 AND end_date >= $2
)duckdb";

constexpr auto kSelectClientByEventQuery = R"duckdb(
SELECT c.*
FROM Client c
JOIN EventClient ec ON c.id = ec.client_id
WHERE ec.event_id = $1
)duckdb";

constexpr auto kSelectClientMonthlyStatsQuery = R"duckdb(
WITH event_stats AS (
    SELECT
        CAST(year(e.start_date) AS INTEGER) AS event_year,
        CAST(month(e.start_date) AS INTEGER) AS event_month,
        COUNT(*) AS sessions,
        COALESCE(SUM(CASE WHEN e.is_work_event THEN COALESCE(e.cost, 0) ELSE 0 END), 0) AS income
    FROM Event e
    JOIN EventClient ec ON ec.event_id = e.id
    WHERE ec.client_id = $1
      AND e.start_date IS NOT NULL
      AND e.start_date >= date_trunc('month', current_timestamp) - (($2 - 1) * INTERVAL '1 month')
    GROUP BY 1, 2
)
SELECT event_year, event_month, sessions, income
FROM event_stats
ORDER BY event_year, event_month
)duckdb";

constexpr auto kSelectDashboardSummaryQuery = R"duckdb(
WITH month_start AS (
    SELECT date_trunc('month', current_timestamp) AS value
)
SELECT
    (SELECT COUNT(*) FROM Client) AS total_clients,
    (SELECT COUNT(*) FROM Client WHERE client_active = TRUE) AS active_clients,
    (SELECT COUNT(*) FROM Event e, month_start ms WHERE e.start_date >= ms.value) AS sessions_this_month,
    (SELECT COUNT(*) FROM Event e, month_start ms WHERE e.start_date >= ms.value AND e.is_work_event = TRUE) AS work_sessions_this_month,
    (SELECT COUNT(*) FROM Event e, month_start ms WHERE e.start_date >= ms.value AND e.is_work_event = FALSE) AS personal_sessions_this_month,
    (SELECT COALESCE(SUM(CASE WHEN e.is_work_event THEN COALESCE(e.cost, 0) ELSE 0 END), 0)
     FROM Event e, month_start ms
     WHERE e.start_date >= ms.value) AS income_this_month
)duckdb";

constexpr auto kSelectDashboardMonthlyStatsQuery = R"duckdb(
SELECT
    CAST(year(e.start_date) AS INTEGER) AS event_year,
    CAST(month(e.start_date) AS INTEGER) AS event_month,
    COUNT(*) AS sessions,
    COUNT(*) FILTER (WHERE e.is_work_event = TRUE) AS work_sessions,
    COUNT(*) FILTER (WHERE e.is_work_event = FALSE) AS personal_sessions,
    COALESCE(SUM(CASE WHEN e.is_work_event THEN COALESCE(e.cost, 0) ELSE 0 END), 0) AS income
FROM Event e
WHERE e.start_date IS NOT NULL
  AND e.start_date >= date_trunc('month', current_timestamp) - (($1 - 1) * INTERVAL '1 month')
GROUP BY 1, 2
ORDER BY 1, 2
)duckdb";

constexpr auto kEventStatus = R"(
INSERT INTO EventStatus (id, name) VALUES
(1, 'pending'),
(2, 'completed'),
(3, 'canceled')
ON CONFLICT (id) DO NOTHING;
)";

constexpr auto kPaymentStatus = R"(
INSERT INTO PaymentStatus (id, name) VALUES
(1, 'pending'),
(2, 'paid'),
(3, 'canceled'),
(4, 'refunded'),
(5, 'skipped')
ON CONFLICT (id) DO NOTHING;
)";

constexpr auto kDemoData = R"(
INSERT INTO Client (id, name, last_name, email, phone_number, client_active, country, city, time_zone, birthday_date, diagnosis, additional_info) VALUES
(1, 'Артём', 'Иванов', 'artem@example.com', '+79001234567', true, 'Россия', 'Москва', 'Europe/Moscow', '1990-05-15', 'Нет', 'Любит утренние тренировки'),
(2, 'Мария', 'Петрова', 'maria@example.com', '+79007654321', true, 'Россия', 'Санкт-Петербург', 'Europe/Moscow', '1985-11-22', 'Астма (в ремиссии)', 'Предпочитает онлайн-встречи'),
(3, 'Алексей', 'Сидоров', 'alex@example.com', '+79001112233', false, 'Казахстан', 'Алматы', 'Asia/Almaty', '1978-03-08', 'Гипертония', 'Занят по будням до 18:00')
ON CONFLICT (id) DO NOTHING;

INSERT INTO Event (id, name, description, is_work_event, event_stat_id, payment_stat_id, start_date, end_date, duration, cost) VALUES
(1, 'Консультация по здоровью', 'Первичная консультация', true, 2, 2, '2025-10-20 10:00:00', '2025-10-20 11:00:00', 3600, 3500.0),
(2, 'Повторный приём', 'Контрольное обследование', true, 1, 1, '2025-11-05 14:00:00', '2025-11-05 15:00:00', 3600, 2800.0),
(3, 'Отменённая сессия', 'Планировалась, но отменена', false, 3, 3, '2025-10-25 09:00:00', '2025-10-25 10:00:00', 3600, NULL),
(4, 'Групповой воркшоп', 'Йога и дыхание', false, 2, 2, '2025-10-30 18:00:00', '2025-10-30 19:30:00', 5400, NULL)
ON CONFLICT (id) DO NOTHING;

INSERT INTO EventClient (id, client_id, event_id)
SELECT 1, 1, 1 WHERE NOT EXISTS (SELECT 1 FROM EventClient WHERE client_id = 1 AND event_id = 1);

INSERT INTO EventClient (id, client_id, event_id)
SELECT 2, 1, 2 WHERE NOT EXISTS (SELECT 1 FROM EventClient WHERE client_id = 1 AND event_id = 2);

INSERT INTO EventClient (id, client_id, event_id)
SELECT 3, 2, 1 WHERE NOT EXISTS (SELECT 1 FROM EventClient WHERE client_id = 2 AND event_id = 1);

INSERT INTO EventClient (id, client_id, event_id)
SELECT 4, 2, 4 WHERE NOT EXISTS (SELECT 1 FROM EventClient WHERE client_id = 2 AND event_id = 4);

INSERT INTO EventClient (id, client_id, event_id)
SELECT 5, 3, 3 WHERE NOT EXISTS (SELECT 1 FROM EventClient WHERE client_id = 3 AND event_id = 3);
)";

} // namespace pcm::database::constance
