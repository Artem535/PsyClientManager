#pragma once

#include <QString>

namespace pcm::confirmation {

QString buildConfirmationRequestText(const QString &clientName,
                                     qint64 startDateTimeUtcMs);
void copyConfirmationRequest(const QString &clientName,
                             qint64 startDateTimeUtcMs);

} // namespace pcm::confirmation
