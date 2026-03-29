# Changelog

All notable changes to this project will be documented in this file.

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
