# Localization Sync (Unify RU/EN Strings) — Design

**Status:** Approved
**Related issue:** #47 ("[P0] Unify UI localization (remove mixed RU/EN strings)")

## Goal

With the interface language set to Russian, no English-only labels or enum
values should remain visible anywhere in the app — and this should stay true
going forward, not just as a one-time cleanup.

## Root cause (from audit)

The codebase is disciplined about wrapping user-facing strings in `tr()` —
essentially no hardcoded literals were found bypassing translation. The bug
is that `translation/app_ru.ts` / `translation/app_en.ts` had not been
regenerated via `lupdate` since 2026-07-24, while several features shipped
after that date with zero translation sync:

- `ClientNotesPage` / `change_log_text.cpp` (notes feed, including the
  event-change-log lines): most of their `tr()` source strings were absent
  from `app_ru.ts` entirely (`Scheduled`, `Paid`, `Status changed: %1 → %2`,
  `Jump to today`, etc.) — falling back to English under a Russian locale.
- `DaySummaryWidget` (day-summary panel): had no entries in `app_ru.ts` at
  all — the whole widget rendered in English regardless of language setting.
- Enum-to-text mappings (event status, payment status) are duplicated across
  a few files (`event_item.cpp`, `qevent_details_widget.cpp`,
  `change_log_text.cpp`) but use identical source strings everywhere — so
  wording is consistent; the failure was purely missing translation entries,
  not term drift.

There is no `lupdate` step anywhere in `CMakeLists.txt`, CI, or documented
workflow — `qt_add_translations(... TS_FILES ...)` only compiles the existing
`.ts` files into `.qm` at build time, it never regenerates `.ts` from source.
Translation sync has been a fully manual, unenforced step, which is why the
drift accumulated silently.

## Scope

1. Run `lupdate` once to catch up `app_ru.ts`/`app_en.ts` with every `tr()`
   string currently in source, and write Russian translations for all of
   them, matching existing terminology already established in the file.
2. Switch `lupdate` to `-locations none -no-obsolete` going forward, to
   avoid location-path churn (see below) and keep the files lean.
3. Add a CI check that fails a PR if `lupdate` would produce a diff against
   the committed `.ts` files — i.e. any new/changed `tr()` string without a
   matching translation blocks merge.
4. Document the requirement in `AGENTS.md`.

## Out of scope

- Adding new languages (this issue is about consistency of RU/EN, not
  expanding locale coverage — matches the original issue's stated
  out-of-scope).
- The exact strings "Title"/"Apply" cited in the original issue text were
  not found verbatim anywhere in `src/`; the issue's wording likely predates
  a since-changed UI label. Not chased further — the systematic sync below
  covers the actual current gap.

## Design

### 1. Catch-up translation pass

Run the CMake-generated `update_translations` target (backed by
`qt_add_translations`, which already correctly scans the whole project via
CMake's dependency graph — verified by a real trial run that found 101 new
source strings). Write Russian translations for every resulting
`type="unfinished"` entry in `app_ru.ts`, reusing existing terminology from
already-translated strings for the same concepts. Rebuild the `.qm` files
and manually verify in the running app with language set to Russian,
specifically the two areas confirmed broken by the audit: the client Notes
feed (including change-log lines) and the day-summary panel.

### 2. `-locations none -no-obsolete`

A trial `lupdate` run surfaced a real problem: one string sourced from a Qt
Designer `.ui` form got a `<location>` pointing into
`build/.../.qt/<hash>/...` — a path that changes per machine/CI run and
would churn on every commit. Since translators here are the dev/agent
working directly in the repo (not an external localization team relying on
jump-to-source), location metadata isn't pulling its weight. Adding
`LUPDATE_OPTIONS -locations none -no-obsolete` to the `qt_add_translations()`
call in the root `CMakeLists.txt` removes `<location>` tags from all
entries (present and future) and drops obsolete entries automatically. This
is a one-time large-looking diff (strips `<location>` from all ~260
existing entries) but permanently avoids the churn, and keeps future diffs
scoped to actual source/translation text changes.

### 3. CI safeguard

New step in `.github/workflows/cmake-multi-platform.yml`, in the existing
matrixed `build` job, gated `if: runner.os == 'Linux'` (translations aren't
platform-specific, so checking once is sufficient), placed immediately
after the existing `Configure` step so a translation gap fails fast instead
of waiting through the full build (macOS legs alone take ~25 minutes):

```yaml
- name: Check translations are up to date
  if: runner.os == 'Linux'
  run: |
    cmake --build build-release --target update_translations
    git diff --exit-code -- translation/app_ru.ts translation/app_en.ts
```

`git diff --exit-code` fails the step (and blocks the PR) if `lupdate`
produces any change against the committed files — whether that's a new
untranslated string or a stale/obsolete one that should have been pruned.

### 4. Docs

Add a line to `AGENTS.md`'s Issue and Branch Workflow section: before
committing new or changed `tr()` strings, run
`cmake --build build-release --target update_translations` and translate
any resulting `type="unfinished"` entries.

## Testing plan

- Manual: switch Settings → Language to Russian, visually confirm the Notes
  feed (notes, session cards, change-log lines) and the day-summary panel
  show Russian text with no English leaking through.
- `ctest --test-dir build --output-on-failure`: full suite still green
  (no test depends on specific `tr()` output text).
- CI: the new "Check translations are up to date" step passes on this
  branch's own commit (proving the catch-up pass is actually complete), and
  a manual smoke check that it *would* fail if a string were added without
  translating it (verified locally before pushing, not by shipping a
  deliberately-broken commit).

## Version/changelog

Per `AGENTS.md`, this MR bumps `CMakeLists.txt` / `application.cpp` and adds
a `CHANGELOG.md` entry, and closes #47 (`Closes #47`).
