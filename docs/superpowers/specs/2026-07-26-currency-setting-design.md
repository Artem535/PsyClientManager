# Currency Selection in Settings — Design

## Goal

Cost is stored as a plain `DOUBLE` with no currency field anywhere in the
schema. The currency symbol shown next to cost values is hardcoded as the
ruble sign (`" ₽"`) in four places:

- `src/app/settings_dialog.cpp` — default work event cost spinbox
- `src/event_view/event_item.cpp` — `formatEventCost()`, timeline display
- `src/pages/analytics_page/analytics_page.cpp` — `formatCurrency()`,
  analytics summary cards
- `src/pages/event_info_page/qevent_details_widget.cpp` — event editor cost
  spinbox

Add a "Currency" setting so the user can pick which symbol is shown. This is
a display-formatting change only — no multi-currency conversion, no
per-record currency, no change to how costs are stored or computed.

Out of scope: currency conversion, per-client/per-event currency, changing
number-grouping locale behavior (the existing `QLocale(QLocale::Russian)`
grouping in `formatEventCost()` is left as-is — only the trailing symbol
changes).

## Data Model

Add to `pcm::app_settings` (mirrors the existing `languageCode()` /
`setLanguageCode()` pair exactly):

```cpp
// app_settings.h
QString currencyCode();
void setCurrencyCode(const QString &code);
QString currencySymbol();
```

- `currencyCode()` / `setCurrencyCode()`: persisted via `QSettings`, key
  `"ui/currency"`, default `"RUB"`.
- `currencySymbol()`: maps the stored code to its display symbol via a small
  lookup in `app_settings.cpp`:

  | Code | Symbol |
  |------|--------|
  | `RUB` | `₽` |
  | `USD` | `$` |
  | `EUR` | `€` |
  | `GBP` | `£` |

  An unrecognized or empty code falls back to `₽` (same as today's
  hardcoded default).

## Settings UI

In `settings_dialog.cpp`, add `mCurrencyCombo` (`QComboBox`) to the
"Timeline colors" group (`eventsBox`) in the Events section, placed right
before the existing "Default work event cost" row — grouped with the other
cost-related controls, following the exact `mLanguageCombo` pattern already
in this file:

```cpp
mCurrencyCombo->addItem(tr("Russian Ruble (₽)"), QStringLiteral("RUB"));
mCurrencyCombo->addItem(tr("US Dollar ($)"), QStringLiteral("USD"));
mCurrencyCombo->addItem(tr("Euro (€)"), QStringLiteral("EUR"));
mCurrencyCombo->addItem(tr("British Pound (£)"), QStringLiteral("GBP"));
```

- `loadSettings()`: selects the combo item matching
  `pcm::app_settings::currencyCode()` (falling back to index 0, same
  `findData()` pattern as `mLanguageCombo`).
- `connectSignals()`: on `currentIndexChanged`, calls
  `pcm::app_settings::setCurrencyCode(mCurrencyCombo->itemData(index).toString())`.

## Call Sites

All four hardcoded `" ₽"` / `tr(" ₽")` literals are replaced with
`pcm::app_settings::currencySymbol()`, suffix position unchanged (always a
trailing suffix, per the existing behavior — no per-currency prefix/suffix
logic):

1. `settings_dialog.cpp:269` —
   `mDefaultWorkCostSpinBox->setSuffix(" " + pcm::app_settings::currencySymbol());`
2. `event_item.cpp:33` — `formatEventCost()` keeps
   `QLocale(QLocale::Russian).toString(*cost, 'f', 0)` for number grouping;
   only the trailing `QStringLiteral(" ₽")` becomes
   `QStringLiteral(" ") + pcm::app_settings::currencySymbol()`. Already
   includes `../widgets/app_settings.h`.
3. `analytics_page.cpp:71` — `formatCurrency()` becomes
   `QLocale().toString(rounded) + QStringLiteral(" ") + pcm::app_settings::currencySymbol()`.
   Needs a new `#include "../../widgets/app_settings.h"` (not currently
   included in this file).
4. `qevent_details_widget.cpp:171` —
   `mUI->mCostSpinBox->setSuffix(" " + pcm::app_settings::currencySymbol());`
   Already includes `../../widgets/app_settings.h`.

## Refresh Behavior

No extra plumbing is needed for the change to apply live:

- `QEventDetailsWidget` (event add/edit form) is constructed fresh inside a
  new modal `QDialog` on every open (`openEventDialog`/
  `openQuickEventDialog` in `event_info.cpp`), so it always reads the
  current currency symbol at construction.
- `SettingsDialog` itself is likewise constructed fresh on every open
  (`MainWindow::openSettingsDialog()`).
- The timeline (`event_item.cpp`) and analytics cards
  (`analytics_page.cpp`) recompute their formatted strings on every
  repaint/`refresh()` call, both of which are already triggered by the
  existing `MainWindow::refreshPageAppearance()` hook (called after the
  Settings dialog closes).

## Testing

No new automated tests. This is a Qt display-formatting change over
already-headless-tested modules (`database`) that this change doesn't
touch; `src/app`/`src/pages` have no existing test coverage, matching this
repo's established UI-untested scope (`docs/asciidoc/07-testing.adoc`).
Verification is manual: open Settings, change currency, confirm the
default-cost field, an existing timeline event, the analytics income card,
and a newly opened event editor all show the new symbol.

## File Changes Summary

- Modify: `src/widgets/app_settings.h`, `src/widgets/app_settings.cpp` (add
  `currencyCode()` / `setCurrencyCode()` / `currencySymbol()`)
- Modify: `src/app/settings_dialog.h`, `src/app/settings_dialog.cpp` (new
  `mCurrencyCombo`, load/save wiring)
- Modify: `src/event_view/event_item.cpp` (`formatEventCost()`)
- Modify: `src/pages/analytics_page/analytics_page.cpp` (`formatCurrency()`,
  new include)
- Modify: `src/pages/event_info_page/qevent_details_widget.cpp`
  (`mCostSpinBox` suffix)
