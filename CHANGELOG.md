# Changelog

All notable changes to this project will be documented in this file.

## [0.1.5] - 2026-07-05

### Added

- recurring event series with daily, weekly, monthly, and yearly repeat options
- repeat controls in the event form, including weekday selection for weekly series
- timeline workflows for creating events from empty slots and editing recurring sessions

### Changed

- event editing now supports series-aware updates and deletion of this or future sessions
- Qlementine combobox popup sizing was patched for event dialogs
- application and release metadata were synchronized to version `0.1.5`

## [0.1.4] - 2026-07-03

### Added

- online sessions through externally managed meeting links
- meeting URL storage, opening, link copying, and invite copying from event forms
- online-session indicators and context-menu actions on timeline event cards
- configurable online-session invite template in settings

### Changed

- application and release metadata were synchronized to version `0.1.4`

## [0.1.3] - 2026-04-27

### Changed

- macOS release packaging now builds the DMG from the installed `.app` bundle
- CI now validates the packaged macOS bundle contents before publishing artifacts
- application and release metadata were synchronized to version `0.1.3`

## [0.1.2] - 2026-03-29

### Added

- client notes workspace
- quick session slots
- payment status tracking for work sessions
- session reminder settings and desktop notifications
- system tray menu with restore and full quit actions

### Changed

- income analytics now count only paid work sessions
- release packaging workflow for Linux, Windows, and macOS was polished
- Russian and English translations were expanded for payment, tray, and reminder flows

### Fixed

- updating an existing event time now persists correctly in the database
- event editing flow no longer depends on passing live `QEventItem*` outside the graphics scene
- multiple crashes around event editing, scene refresh, and tray lifecycle were fixed

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
