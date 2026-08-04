#pragma once

#include "schema.hpp"

#include <QString>

namespace pcm::client_notes {

// Human-readable label for an Event.event_stat_id value, as used in the
// client notes feed's change-log lines.
QString eventChangeStatusLabel(int64_t eventStatusId);

// Human-readable label for an Event.payment_stat_id value, as used in the
// client notes feed's change-log lines.
QString eventChangePaymentLabel(int64_t paymentStatusId);

// Builds the display text for a single EventChangeLog entry
// (status change / payment change / reschedule). Returns an empty string
// for an unrecognized change_kind.
QString changeLogLineText(const DuckEventChangeLog &entry);

} // namespace pcm::client_notes
