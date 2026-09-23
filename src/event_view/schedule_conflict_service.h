#pragma once

#include <optional>

#include <QVector>

#include "schema.hpp"

namespace pcm::schedule {

[[nodiscard]] bool hasConflict(const DuckEvent &candidate,
                               const QVector<DuckEvent> &events);

// Same overlap/buffer/self/occurrence rules as hasConflict, but returns the
// first conflicting event (in `events` order) instead of just a bool, so
// callers can name it in a message.
[[nodiscard]] std::optional<DuckEvent> findConflict(const DuckEvent &candidate,
                                                    const QVector<DuckEvent> &events);

} // namespace pcm::schedule
