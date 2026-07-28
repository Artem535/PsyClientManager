# RestoreService Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Restore a validated `.psybackup` archive into a local database directory without exposing partially restored data.

**Architecture:** `RestoreService` reuses `BackupValidator`, extracts into a temporary staging area, imports the DuckDB export into a fresh database directory, and swaps staged directories into place only after all checks pass. Existing database and attachments directories are renamed to protective pre-restore paths before replacement and restored on failure.

**Tech Stack:** C++20, DuckDB, Poco Foundation/Zip, reflect-cpp, GoogleTest.

## Global Constraints

- Never modify the destination before validation and staging complete.
- Reject unsupported backup format and schema versions.
- Require an explicit attachments destination when the archive contains attachments.
- Preserve the previous database and attachments as protective copies after success.
- Restore the previous directories if any replacement step fails.
- Update version, README, CHANGELOG, and AsciiDoc in the PR.

### Task 1: Restore contract and extraction helpers (completed)

**Files:** Create `src/backup/restore_service.h/.cpp`; modify `src/backup/CMakeLists.txt`.

- Add `RestoreOptions { std::optional<std::string> attachments_root; }`.
- Add `RestoreResult { bool ok; std::string error; std::string protective_database_path; std::string protective_attachments_path; }`.
- Add `RestoreService::restore_backup(const std::string&, const std::string&, const RestoreOptions&)`.
- Validate with `BackupValidator` before extraction.
- Extract to a UUID-named temporary directory and require `database/schema.sql`, `database/load.sql`, and at least one database export file.

### Task 2: Staged import and directory swap (completed)

**Files:** Modify `src/backup/restore_service.cpp`; test `test/backup_tests.cpp`.

- Import the staged export with DuckDB `IMPORT DATABASE '<staged database>'`.
- Copy optional attachments into a staged attachments directory.
- Rename existing destination directories to `.pre-restore-<uuid>`.
- Rename staged directories into place and rollback all completed renames if a later operation fails.
- Keep protective copies on success.

### Task 3: Regression tests and release metadata (completed)

**Files:** Modify `test/backup_tests.cpp`, `CMakeLists.txt`, `src/app/application.cpp`, `CHANGELOG.md`, `README.md`, and `docs/asciidoc/05-modules.adoc`.

- Test database round-trip, attachment round-trip, invalid archive rejection, and rollback preservation.
- Bump the application to `0.1.9` and document the restore service.
- Run build, all tests, and `git diff --check` before opening the PR.
