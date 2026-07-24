# Schedule Conflict Service Implementation Plan

> **For agentic workers:** Implement this plan task-by-task with verification after each task.

**Goal:** Extract deterministic event-overlap rules from `QTimelineModel` into a reusable `ScheduleConflictService` without changing current recurring-event semantics.

**Architecture:** The service is a pure function over a candidate `DuckEvent` and a collection of already loaded events. It ignores the candidate itself and its materialized recurring occurrence, and treats half-open time ranges as overlapping only when `start < other.end && end > other.start`. Database-wide conflict lookup remains the fallback for events not present in the loaded timeline.

**Tech Stack:** C++20, Qt 6, GoogleTest, existing `DuckEvent` model.

## Global Constraints

- Do not change the database schema.
- Do not add buffer semantics yet.
- Preserve virtual occurrence and materialized override behavior.
- Do not stage or modify unrelated `third_party/qlementine` changes.

### Task 1: Add failing service tests

**Files:**
- Create: `test/schedule_conflict_service_tests.cpp`
- Modify: `test/CMakeLists.txt`

Add tests for overlapping ranges, adjacent ranges, missing timestamps, self-update,
and the same recurring occurrence. The test should use real `DuckEvent` values and
fail because the service does not exist yet.

Run:

```bash
cmake --build build --target PsyClientManager_schedule_conflict_tests --parallel
ctest --test-dir build -R ScheduleConflict --output-on-failure
```

Expected: compilation or test failure due to the missing service.

### Task 2: Implement the pure service

**Files:**
- Create: `src/event_view/schedule_conflict_service.h`
- Create: `src/event_view/schedule_conflict_service.cpp`
- Modify: `src/event_view/CMakeLists.txt`

Expose:

```cpp
namespace pcm::schedule {
bool hasConflict(const DuckEvent &candidate,
                 const QVector<DuckEvent> &events);
}
```

Use half-open ranges. Return `false` when either event lacks start or end. Skip an
event with the same positive id as the candidate. Also skip two events representing
the same `(series_id, original_occurrence_start)` occurrence, including a virtual
occurrence and its materialized override.

Run the focused test and expect it to pass.

### Task 3: Connect `QTimelineModel`

**Files:**
- Modify: `src/event_view/qtimeline_model.cpp`

Remove the local overlap helpers and delegate the loaded-event check to
`pcm::schedule::hasConflict`. Keep `Database::has_conflict` as the fallback for
events not represented in the current day model.

Run the full build and database/config tests.

### Task 4: Verify and document

**Files:**
- Modify: `docs/asciidoc/05-modules.adoc` if the module list needs the service;
- Modify: `CHANGELOG.md` only when the release boundary is prepared.

Run `git diff --check`, focused tests, all configured tests and a release build.
Do not change the version for this isolated implementation task.
