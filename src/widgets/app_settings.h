#pragma once

#include <QColor>
#include <QString>

namespace pcm::app_settings {

bool confirmEventDeletion();
void setConfirmEventDeletion(bool enabled);

bool showStatusBarMessages();
void setShowStatusBarMessages(bool enabled);

QString languageCode();
void setLanguageCode(const QString &languageCode);

QColor workEventColor();
void setWorkEventColor(const QColor &color);

QColor personalEventColor();
void setPersonalEventColor(const QColor &color);

} // namespace pcm::app_settings
