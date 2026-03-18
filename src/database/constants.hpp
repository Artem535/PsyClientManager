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
