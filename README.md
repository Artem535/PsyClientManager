# PsyClientManager

Desktop application for client management, scheduling, and lightweight analytics.

Project documentation in AsciiDoc:

- `docs/asciidoc/index.adoc`

## Stack

- `C++20`
- `Qt 6.10`
- `Qlementine`
- `DuckDB`
- `QCustomPlot`
- `vcpkg`
- `CMake`

## Repository Setup

Clone the repository with submodules:

```bash
git clone --recursive <repo-url>
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

The main executable will be produced in `build-release/`.

## Dependencies

Manifest dependencies are declared in:

- `vcpkg.json`
- `vcpkg-configuration.json`

The project expects a working `VCPKG_ROOT`.

## Qlementine

`Qlementine` is tracked as a git submodule in `third_party/qlementine`.

The project applies a local compatibility patch during CMake configure:

- patch file: `patches/qlementine/0001-guard-widget-animation-manager-on-destroy.patch`
- patch script: `scripts/apply_qlementine_patch.cmake`

No manual patch step is required during normal configure/build.

## CI Packaging

GitHub Actions builds packages for both target platforms:

- `Linux`: `AppImage`
- `Windows`: deploy bundle via `windeployqt`

Artifacts are available from the corresponding workflow run in the `Actions` tab.

## Notes

- Tests are disabled by default. To enable them explicitly, configure with:

```bash
cmake --preset vcpkg-release -DPCM_BUILD_TESTS=ON
```

- The Windows CI job uses the MSVC toolchain.
- The Linux CI job installs Qt through the official Qt installer and packages the app as `AppImage`.
