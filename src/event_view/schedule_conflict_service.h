#pragma once

#include <QVector>

#include "schema.hpp"

namespace pcm::schedule {

[[nodiscard]] bool hasConflict(const DuckEvent &candidate,
                               const QVector<DuckEvent> &events);

} // namespace pcm::schedule
