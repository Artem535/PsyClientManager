# PsyClientManager

PsyClientManager is a desktop application for scheduling sessions, managing clients, and tracking lightweight practice analytics.

It is built as a native Qt Widgets application and targets Linux, Windows, and macOS.

## Highlights

- Calendar-based session planning
- Client list with search and detail pages
- Work and personal event separation
- Session cost tracking
- Analytics dashboard with charts
- Dark UI powered by Qlementine
- Cross-platform packaging through GitHub Actions

## Current Features

- Calendar page with custom month view
- Timeline view for daily events
- Creation and editing of work and personal events
- Linked client selection for work events
- Default pricing for work sessions
- Optional prevention of overlapping events
- Client list with search and quick actions
- Client detail page with charts
- Analytics dashboard with summary cards and plots
- Language selection in settings
- Configurable colors for work and personal events
- Quick access to the local database folder

## Planned Improvements

- Recurring events
- Richer analytics and business reports
- Better calendar interactions and editing flows
- Export and backup options
- More detailed client history
- Improved settings and personalization
- Broader release automation and polish for all platforms

## Tech Stack

- `C++20`
- `Qt 6.10`
- `Qlementine`
- `DuckDB`
- `QCustomPlot`
- `vcpkg`
- `CMake`

## Project Layout

- `src/` application code
- `ui/` Qt Designer forms
- `resources/` themes and icons
- `translation/` Qt Linguist translations
- `third_party/` vendored dependencies and submodules
- `packaging/` platform packaging assets
- `docs/asciidoc/` project documentation

## Getting Started

Clone the repository with submodules:

```bash
git clone --recursive <repo-url>
cd PsyClientManager
```

If the repository is already cloned:

```bash
git submodule update --init --recursive
```

## Local Build

The project uses `vcpkg` through `CMakePresets.json`.

Configure:

```bash
cmake --preset vcpkg-release
```

Build:

```bash
cmake --build build-release --parallel
```

The main binary is produced in `build-release/`.

## Dependencies

Manifest dependencies are defined in:

- `vcpkg.json`
- `vcpkg-configuration.json`

The build expects `VCPKG_ROOT` to point to a valid `vcpkg` checkout.

## Qlementine Integration

`Qlementine` is tracked as a submodule in `third_party/qlementine`.

The repository applies a local compatibility patch automatically during CMake configure:

- patch: `patches/qlementine/0001-guard-widget-animation-manager-on-destroy.patch`
- script: `scripts/apply_qlementine_patch.cmake`

No manual patch step is required during normal builds.

## Tests

Tests are disabled by default.

To enable them explicitly:

```bash
cmake --preset vcpkg-release -DPCM_BUILD_TESTS=ON
cmake --build build-release --parallel
```

## Packaging

GitHub Actions builds platform packages for:

- `Linux` as `AppImage`
- `Windows` as an `Inno Setup` installer
- `macOS` as `DMG`

Build artifacts are attached to workflow runs in the `Actions` tab.

## Releases

Publishing is configured only for merges to `main`.

The release workflow:

1. builds all supported platform packages
2. reads the application version from `CMakeLists.txt`
3. creates or updates a GitHub Release tagged as `v<version>`
4. uploads Linux, Windows, and macOS release artifacts

The application version is defined in:

- `CMakeLists.txt`

## Roadmap Notes

The project is under active development. The current focus is on:

- stabilizing cross-platform packaging
- improving day-to-day scheduling UX
- expanding analytics
- preparing a cleaner release flow for public builds

## Documentation

Additional documentation lives in:

- `docs/asciidoc/index.adoc`

## License

This repository is distributed under `GPLv3`.
