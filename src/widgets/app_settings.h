#pragma once

#include <QColor>
#include <QString>
#include <QTime>

namespace pcm::app_settings {

bool confirmEventDeletion();
void setConfirmEventDeletion(bool enabled);
bool preventEventOverlaps();
void setPreventEventOverlaps(bool enabled);

bool showStatusBarMessages();
void setShowStatusBarMessages(bool enabled);

QString languageCode();
void setLanguageCode(const QString &languageCode);

QColor workEventColor();
void setWorkEventColor(const QColor &color);

QColor personalEventColor();
void setPersonalEventColor(const QColor &color);

double defaultWorkEventCost();
void setDefaultWorkEventCost(double cost);

QTime workDayStart();
void setWorkDayStart(const QTime &time);

QTime workDayEnd();
void setWorkDayEnd(const QTime &time);

int defaultSessionDurationMinutes();
void setDefaultSessionDurationMinutes(int minutes);

} // namespace pcm::app_settings
