# Local Backup Service — Design

Closes #10.

## Goal

Add a local `.psybackup` backup format for the DuckDB database and (optionally)
client note attachments. A `BackupService` writes a backup to an explicit
destination path, atomically, without ever copying the live DuckDB files as an
unsafe raw snapshot. A `BackupValidator` can verify a `.psybackup` file's
integrity after the fact.

Out of scope: encryption, cloud upload, restore UI (restore itself is not
implemented — only creation and validation).

## Format

`.psybackup` is a real ZIP file (not a bare directory), created with
`Poco::Zip::Compress`. Enabling Poco's vcpkg `zip` feature adds no new
third-party dependency — it only pulls in zlib, which Poco already requires.

Archive contents:

```
manifest.json
database/schema.sql
database/load.sql
database/<table>.parquet   (one per table)
attachments/...            (only present when attachments were included;
                             mirrors each attachment's relative_path)
```

### `manifest.json`

Serialized via reflect-cpp (`rfl::json`), matching how `Config` already uses
reflect-cpp for YAML:

```json
{
  "psybackup_format_version": 1,
  "created_at": 1732550400000,
  "workspace_uuid": "...",
  "schema_version": 1,
  "backup_format_version": 1,
  "kind": "database" | "database_and_attachments",
  "entries": [
    { "path": "database/Client.parquet", "size_bytes": 4821, "sha256": "..." }
  ]
}
```

`entries` lists every archive member except `manifest.json` itself.
`workspace_uuid`, `schema_version`, and `backup_format_version` come from
`Database::get_application_metadata()`.

## Module Layout

New module `src/backup/`, following the existing `src/database` / `src/config`
pattern: own `CMakeLists.txt`, Qt-free, tested with plain gtest (like
`test/database_tests.cpp`).

- `backup_manifest.hpp` — `BackupManifest` / `BackupEntry` DTOs.
- `checksum_utils.hpp` — `sha256_file(path)` via `Poco::SHA2Engine` (Poco
  Foundation, no OpenSSL needed).
- `backup_service.h/.cpp` — `BackupService`.
- `backup_validator.h/.cpp` — `BackupValidator`.

One addition to the existing `Database` class:

```cpp
bool export_snapshot(const std::string &target_dir) const;
```

Opens a connection and runs `EXPORT DATABASE '<target_dir>' (FORMAT PARQUET);`.
This keeps all DuckDB/SQL specifics inside `Database`, matching its existing
ownership of the schema and queries.

## Why `EXPORT DATABASE` instead of a file copy

DuckDB 1.4.4 (vendored) provides `EXPORT DATABASE ... (FORMAT PARQUET)`, a
transactionally consistent export run through a connection. This satisfies the
acceptance criterion that "open database files are not copied as an unsafe raw
file snapshot" directly — the alternative (checkpoint + raw `.db`/WAL file
copy) is exactly what that criterion rules out. It also produces natural
per-table files to checksum, and doesn't tie the backup format to DuckDB's
internal storage format version the way `COPY FROM DATABASE ... TO ...` into
another `.duckdb` file would.

## `BackupService::create_backup` Pipeline

Signature (illustrative):

```cpp
struct BackupOptions {
  std::optional<std::string> attachments_root; // nullopt => database-only
};

struct BackupResult {
  bool ok = false;
  std::string error; // empty when ok
};

BackupResult create_backup(const Database &db,
                            const std::string &destination_path,
                            const BackupOptions &options = {});
```

Steps:

1. Create a scratch directory under `Poco::Path::temp()` named with a UUID
   (`Poco::UUIDGenerator`, same pattern already used for `workspace_uuid`).
2. `db.export_snapshot(scratch/"database")`.
3. If `options.attachments_root` is set and exists, recursively copy its tree
   into `scratch/"attachments"`.
4. Walk every file under `scratch`, compute SHA-256 for each, build the
   `BackupManifest`, write `scratch/manifest.json`.
5. Zip `scratch`'s contents into a temp file **in the same directory as
   `destination_path`** (e.g. `destination_path + ".partial-<uuid>"`) via
   `Poco::Zip::Compress`.
6. On success: `Poco::File(tempFile).renameTo(destination_path)` — atomic on
   the same filesystem. `destination_path` is touched exactly once, at the
   very end, satisfying "failed backup does not replace an existing backup."
7. On any failure at any step: delete the temp file/scratch dir, leave
   `destination_path` untouched, return `BackupResult{false, "..."}`. No
   exceptions escape `create_backup`.
8. Scratch directory is always cleaned up (RAII guard), success or failure.

## `BackupValidator`

```cpp
struct ValidationResult {
  bool ok = false;
  std::vector<std::string> errors;
};

ValidationResult validate(const std::string &backup_path);
```

1. Open the zip with `Poco::Zip::Decompress`, extract and parse
   `manifest.json` via rfl.
2. For every manifest entry: confirm the zip contains that path, stream it,
   recompute SHA-256, compare hash and size.
3. Flag archive members not listed in the manifest, and manifest entries
   missing from the archive.
4. Reject an unrecognized `psybackup_format_version`.

This satisfies "corrupted files are detected by validation."

## Testing

New `test/backup_tests.cpp`, registered under `PCM_BUILD_TESTS`, using temp
dirs under `Poco::Path::current()` (same pattern as `database_tests.cpp`):

- Database-only backup: file created, `BackupValidator` reports ok, entries
  match the exported tables, `kind == "database"`.
- Database+attachments backup: `attachments/` present in the archive with
  correct checksums, `kind == "database_and_attachments"`.
- Corrupting a byte in a produced `.psybackup` causes validation to fail with
  a checksum mismatch error.
- A failure injected mid-pipeline (e.g. an unwritable destination directory)
  leaves a pre-existing backup at `destination_path` byte-for-byte unchanged,
  and leaves no partial file at the destination.
- `get_application_metadata()`'s `backup_format_version` flows into the
  manifest unchanged.

## Docs / Version / Changelog

Per this repo's contributor workflow, the PR also updates:

- `CMakeLists.txt` — version bump.
- `CHANGELOG.md` — new entry.
- `README.md` — mention local backups if it documents features at that level.
- `docs/asciidoc/05-modules.adoc` — new `src/backup` section.
- `docs/asciidoc/06-database-and-data-model.adoc` — forward-reference from the
  existing `backup_format_version` note to the new module.
- `vcpkg.json` — enable Poco's `zip` feature (`{"name": "poco", "features":
  ["zip"]}`).
