# Changelog

All notable changes to this project will be documented in this file.

## 0.1.1 - 2026-03-19

### Added
- Client notes workspace with a dedicated `Notes` page.
- Markdown note rendering in a self-chat style timeline.
- File and image attachments for notes.
- Image previews and quick file opening from notes.
- Notes entry action directly in the client list card.
- New `notes` icon and sidebar navigation support.

### Improved
- Better separation of client actions: `Details`, `Notes`, and `Delete` now work independently.
- Notes layout, bubble sizing, and long-text wrapping.
- Client list action column sizing for three actions.
- Shared widget constants for notes layout.

### Internal
- Added `ClientNote` and `ClientNoteAttachment` persistence in DuckDB.
- Attachments are stored on disk, with metadata kept in the database.
- Added `en` and `ru` translations for notes-related UI.
- Project version bumped to `0.1.1`.
