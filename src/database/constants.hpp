#pragma once

namespace pcm::database::constance {

constexpr auto kEventStatus = R"(
INSERT INTO ObxEventStatus (id, name) VALUES
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
(5, 'skipped');
)";

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
    event_stat_id BIGINT REFERENCES ObxEventStatus(id),
    payment_stat_id BIGINT REFERENCES ObxPaymentStatus(id),
    start_date TIMESTAMP,
    end_date TIMESTAMP,
    duration BIGINT
);

-- Many-to-many relationship between clients and events
CREATE TABLE IF NOT EXISTS EventClient (
    id INTEGER PRIMARY KEY,
    client_id BIGINT NOT NULL REFERENCES ObxClient(id),
    event_id BIGINT NOT NULL REFERENCES ObxEvent(id),
    UNIQUE (client_id, event_id)
);
)duckdb";

}; // namespace pcm::database::constance
