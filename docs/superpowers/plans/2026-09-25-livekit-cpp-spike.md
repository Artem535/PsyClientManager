# LiveKit C++/Qt Capture-and-Render Spike Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an isolated, throwaway Qt/C++ executable under `spikes/livekit-cpp-spike/` that proves the LiveKit C++ SDK can be driven by a purely Qt Multimedia capture/render pipeline, gating issue #76 (P1-B) per `docs/video-roadmap.en.md` §5.3.

**Architecture:** A single Qt Widgets executable (`livekit_spike`) links `LiveKit::livekit` (downloaded as a prebuilt release via a vendored `LiveKitSDK.cmake` helper, not vcpkg) and `Qt6::Multimedia`/`Qt6::OpenGLWidgets`. `QCamera`/`QAudioSource` capture local media, convert frames to LiveKit's `VideoFrame`/`AudioFrame` types, and publish them via `livekit::VideoSource`/`AudioSource`. A `QOpenGLWidget` renders remote video pulled via a blocking `VideoStream::read()` loop on a dedicated thread, marshaled to the Qt UI thread.

**Tech Stack:** Qt 6.10 (Widgets, Multimedia, OpenGLWidgets), LiveKit C++ SDK 1.12.0 (Apache 2.0, prebuilt release), CMake 3.28, GoogleTest (for pure-function tests only, no `QApplication`).

## Global Constraints

- Code lives entirely under `spikes/livekit-cpp-spike/`. It is never `add_subdirectory`'d from the main project's `CMakeLists.txt` and never links into `PsyClientManager_app`. It does not touch `vcpkg.json`.
- Capture and render go through Qt Multimedia only (`QCamera`, `QAudioSource`, `QOpenGLWidget`). No platform-native fallback code (V4L2/Media Foundation/AVFoundation) is added anywhere in this plan — if Qt Multimedia cannot do the job on some OS, that is a decision-gate finding for the human to act on, not something to route around.
- `LIVEKIT_URL`/`LIVEKIT_TOKEN` are read from environment variables at process start. Nothing is hardcoded or committed. The executable must run and be useful (local capture/preview) even when these are unset — it only skips the room-connect step.
- No `QApplication`-based test harness is introduced (matches the main repo's existing convention). Only pure-function logic (no Qt widgets, no LiveKit network calls) gets a GoogleTest case.
- Every new file needs a short SPDX-style or plain comment noting it's spike/throwaway code — no other repo convention (translations, CHANGELOG, version bump) applies to this directory; it ships nothing to end users.
- `LiveKitSDK.cmake` is a vendored third-party file (Apache 2.0, from `livekit-examples/cpp-example-collection`, commit `7abd4b97aebd33a681cfbbed9b739fdc5c2c17e3`) — copy it verbatim, do not hand-edit it.

---

### Task 1: Spike scaffold, SDK download, and a minimal buildable executable

**Files:**
- Create: `spikes/livekit-cpp-spike/CMakeLists.txt`
- Create: `spikes/livekit-cpp-spike/cmake/LiveKitSDK.cmake`
- Create: `spikes/livekit-cpp-spike/src/main.cpp`
- Create: `spikes/livekit-cpp-spike/README.md`

**Interfaces:**
- Produces: the `livekit_spike` CMake executable target, configurable standalone via `cmake -S spikes/livekit-cpp-spike -B spike-build && cmake --build spike-build`. Later tasks add source files to this target but never change how it's configured.

- [ ] **Step 1: Fetch the vendored `LiveKitSDK.cmake` helper**

```bash
mkdir -p spikes/livekit-cpp-spike/cmake
curl -sL \
  "https://raw.githubusercontent.com/livekit-examples/cpp-example-collection/7abd4b97aebd33a681cfbbed9b739fdc5c2c17e3/cmake/LiveKitSDK.cmake" \
  -o spikes/livekit-cpp-spike/cmake/LiveKitSDK.cmake
sha256sum spikes/livekit-cpp-spike/cmake/LiveKitSDK.cmake
```

Expected sha256: `24ffdeaf39b7fb9ae44e379beaaf6c38e00ef647cb99cb8945772c495dc26660`. If it doesn't match, stop and report BLOCKED — do not proceed with an unverified third-party CMake script.

- [ ] **Step 2: Create `spikes/livekit-cpp-spike/CMakeLists.txt`**

```cmake
# Throwaway spike for issue #77 — see docs/superpowers/specs/2026-09-25-livekit-cpp-spike-design.md
# Not part of the main PsyClientManager build; never add_subdirectory'd from the top-level CMakeLists.txt.
cmake_minimum_required(VERSION 3.28)
project(livekit_cpp_spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(LIVEKIT_SDK_VERSION "1.12.0" CACHE STRING "LiveKit C++ SDK version")

include(LiveKitSDK)
livekit_sdk_setup(
  VERSION "${LIVEKIT_SDK_VERSION}"
  SDK_DIR "${CMAKE_BINARY_DIR}/_deps/livekit-sdk"
)
find_package(LiveKit CONFIG REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia OpenGLWidgets)
qt_standard_project_setup()

qt_add_executable(livekit_spike
  src/main.cpp
)

target_link_libraries(livekit_spike PRIVATE
  LiveKit::livekit
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Multimedia
  Qt6::OpenGLWidgets
)

# Make -llivekit_ffi resolvable, matching the upstream example's own pattern
# (spikes/livekit-cpp-spike/cmake/LiveKitSDK.cmake only wires CMAKE_PREFIX_PATH
# for find_package; the actual FFI shared lib lives one directory up from
# lib/cmake/LiveKit).
get_filename_component(_lk_cmake_dir "${LiveKit_DIR}" DIRECTORY)
get_filename_component(_lk_lib_dir "${_lk_cmake_dir}" DIRECTORY)
target_link_directories(livekit_spike PRIVATE "${_lk_lib_dir}")

if(WIN32)
  get_filename_component(_lk_prefix "${_lk_lib_dir}" DIRECTORY)
  set(_lk_bin_dir "${_lk_prefix}/bin")
  foreach(_lk_runtime_dll IN ITEMS livekit.dll livekit_ffi.dll)
    if(EXISTS "${_lk_bin_dir}/${_lk_runtime_dll}")
      add_custom_command(TARGET livekit_spike POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_lk_bin_dir}/${_lk_runtime_dll}"
                "$<TARGET_FILE_DIR:livekit_spike>"
      )
    endif()
  endforeach()
endif()

option(LIVEKIT_SPIKE_BUILD_TESTS "Build the spike's pure-function unit tests" OFF)
if(LIVEKIT_SPIKE_BUILD_TESTS)
  add_subdirectory(test)
endif()
```

- [ ] **Step 3: Create `spikes/livekit-cpp-spike/src/main.cpp`**

```cpp
// Throwaway spike for issue #77 (LiveKit C++/Qt capture-and-render proof).
// Not shipped, not part of the main app build.
#include <QApplication>

#include <cstdlib>
#include <iostream>

#include "livekit/livekit.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  livekit::initialize(livekit::LogLevel::Info);
  std::cout << "LiveKit version: " << LIVEKIT_BUILD_VERSION_FULL << " ("
            << LIVEKIT_BUILD_FLAVOR << ", commit " << LIVEKIT_BUILD_COMMIT
            << ", built " << LIVEKIT_BUILD_DATE << ")" << std::endl;

  const char *url = std::getenv("LIVEKIT_URL");
  const char *token = std::getenv("LIVEKIT_TOKEN");
  if (!url || !token) {
    std::cout << "LIVEKIT_URL / LIVEKIT_TOKEN not set — room connection "
                 "will be skipped in later tasks; SDK init/shutdown still "
                 "verified below."
              << std::endl;
  }

  livekit::shutdown();
  std::cout << "livekit::initialize()/shutdown() round-trip OK." << std::endl;
  return 0;
}
```

This step deliberately does not create a `QMainWindow` yet — it only proves
the SDK links and its init/shutdown cycle works cleanly, and that
`QApplication` can coexist with it. Task 3 replaces this `main.cpp` with the
real window.

- [ ] **Step 4: Configure and build on Linux**

```bash
cmake -S spikes/livekit-cpp-spike -B spike-build
cmake --build spike-build --parallel
```

Expected: configure step downloads and extracts the LiveKit SDK (watch for
`LiveKitSDK: using SDK at ...` in the output), `find_package(LiveKit CONFIG
REQUIRED)` succeeds, and the build produces `spike-build/livekit_spike` with
zero errors. If `find_package(Qt6 ... COMPONENTS Multimedia OpenGLWidgets)`
fails because those modules aren't installed in this environment's Qt copy,
install them (e.g. `apt install qt6-multimedia-dev` on the dev box, or the
equivalent Qt Maintenance Tool components) — do not remove the
`find_package` requirement to work around a missing module.

- [ ] **Step 5: Run it**

```bash
./spike-build/livekit_spike
```

Expected output: the LiveKit version line, the "not set" notice (unless you
already have `LIVEKIT_URL`/`LIVEKIT_TOKEN` exported), and the "round-trip
OK" line, then a clean exit (code 0).

- [ ] **Step 6: Write `spikes/livekit-cpp-spike/README.md`**

```markdown
# LiveKit C++/Qt Capture-and-Render Spike

Throwaway exploratory code for issue #77. Not part of the main
PsyClientManager build — never wired into the top-level CMakeLists.txt.

Design: `docs/superpowers/specs/2026-09-25-livekit-cpp-spike-design.md`
Plan: `docs/superpowers/plans/2026-09-25-livekit-cpp-spike.md`

## Build

    cmake -S spikes/livekit-cpp-spike -B spike-build
    cmake --build spike-build --parallel

## Run

    LIVEKIT_URL=wss://your-project.livekit.cloud \
    LIVEKIT_TOKEN=<a manually generated room JWT> \
    ./spike-build/livekit_spike

Without `LIVEKIT_URL`/`LIVEKIT_TOKEN` set, the app still runs — it just
skips connecting to a room.

## Manual verification checklist (roadmap §5.3's six proof points)

CI (`.github/workflows/cmake-multi-platform.yml`, `Build LiveKit spike`
step) proves point 1 only — that the SDK builds and links on all three
OSes. It cannot exercise a camera, a microphone, or a second machine.
Everything below needs a human on real hardware:

1. Confirm CI's `Build LiveKit spike` step is green on all three OS matrix
   entries. A red step here means the SDK didn't build on that OS —
   everything below is moot for that OS until it's fixed.
2. Launch the spike; confirm camera + microphone capture starts with no
   crash, and no leaked device handle after repeated start/stop (watch
   for the OS's camera-in-use indicator staying lit after the app exits).
3. Confirm local preview and remote video render correctly, including
   during window resize.
4. Mid-session, switch camera, microphone, and audio output device from
   the combo boxes; confirm the stream keeps working.
5. Click Leave, then close the window; confirm tracks stop, no dangling
   callback fires afterward, and the process exits cleanly.
6. Run a real call between two machines over the staging LiveKit Cloud
   project — once on a network with direct connectivity, once forcing
   TURN/TLS.

## Decision gate

Per `docs/video-roadmap.en.md` §5.3: P1-B (#78, #79, #80) proceeds only if
all six points above are reproducible and SDK artifacts can be packaged
for Linux/Windows/macOS. A failed point is an architecture decision to
bring back to the team — not something to quietly route around.
```

- [ ] **Step 7: Commit**

```bash
git add spikes/livekit-cpp-spike/CMakeLists.txt \
        spikes/livekit-cpp-spike/cmake/LiveKitSDK.cmake \
        spikes/livekit-cpp-spike/src/main.cpp \
        spikes/livekit-cpp-spike/README.md
git commit -m "spike(livekit): scaffold isolated CMake target, SDK download, init/shutdown smoke test"
```

---

### Task 2: Pure frame-conversion utilities (audio chunking + QImage→LiveKit VideoFrame)

**Files:**
- Create: `spikes/livekit-cpp-spike/src/audio_chunker.h`
- Create: `spikes/livekit-cpp-spike/src/audio_chunker.cpp`
- Create: `spikes/livekit-cpp-spike/src/frame_convert.h`
- Create: `spikes/livekit-cpp-spike/src/frame_convert.cpp`
- Create: `spikes/livekit-cpp-spike/test/CMakeLists.txt`
- Create: `spikes/livekit-cpp-spike/test/audio_chunker_tests.cpp`
- Create: `spikes/livekit-cpp-spike/test/frame_convert_tests.cpp`
- Modify: `spikes/livekit-cpp-spike/CMakeLists.txt:1-60` (add `src/audio_chunker.cpp` and `src/frame_convert.cpp` to the `qt_add_executable` sources; the `test/` subdirectory is already wired via the `LIVEKIT_SPIKE_BUILD_TESTS` option from Task 1)

**Interfaces:**
- Produces: `pcm::spike::AudioChunker` (framework-free: takes raw `int16_t` PCM bytes, returns fixed-size frames) and `pcm::spike::videoFrameToLiveKitRGBA(const QImage&) -> livekit::VideoFrame` — both consumed by Task 3's capture adapters.

- [ ] **Step 1: Write the failing test for `AudioChunker`**

`AudioChunker` accumulates raw interleaved PCM bytes and yields fixed-size
frames of exactly `samplesPerChannel` int16 samples per channel, buffering
any partial remainder for the next call.

```cpp
// spikes/livekit-cpp-spike/test/audio_chunker_tests.cpp
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "audio_chunker.h"

using pcm::spike::AudioChunker;

TEST(AudioChunkerTest, EmitsNoFramesBelowThreshold) {
  AudioChunker chunker(/*samplesPerChannel=*/4, /*channels=*/1);
  std::vector<int16_t> samples = {1, 2, 3};
  const auto frames = chunker.push(samples);
  EXPECT_TRUE(frames.empty());
}

TEST(AudioChunkerTest, EmitsOneFrameExactly) {
  AudioChunker chunker(/*samplesPerChannel=*/4, /*channels=*/1);
  std::vector<int16_t> samples = {1, 2, 3, 4};
  const auto frames = chunker.push(samples);
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0], (std::vector<int16_t>{1, 2, 3, 4}));
}

TEST(AudioChunkerTest, CarriesRemainderAcrossCalls) {
  AudioChunker chunker(/*samplesPerChannel=*/4, /*channels=*/1);
  EXPECT_TRUE(chunker.push({1, 2, 3}).empty());
  const auto frames = chunker.push({4, 5, 6, 7});
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0], (std::vector<int16_t>{1, 2, 3, 4}));
  const auto frames2 = chunker.push({});
  EXPECT_TRUE(frames2.empty());
}

TEST(AudioChunkerTest, EmitsMultipleFramesFromOnePush) {
  AudioChunker chunker(/*samplesPerChannel=*/2, /*channels=*/1);
  const auto frames = chunker.push({1, 2, 3, 4, 5});
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(frames[0], (std::vector<int16_t>{1, 2}));
  EXPECT_EQ(frames[1], (std::vector<int16_t>{3, 4}));
}

TEST(AudioChunkerTest, StereoFrameSizeIsSamplesPerChannelTimesChannels) {
  AudioChunker chunker(/*samplesPerChannel=*/2, /*channels=*/2);
  // 2 samples/channel * 2 channels = 4 interleaved int16 values per frame.
  const auto frames = chunker.push({1, 2, 3, 4, 5, 6});
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0], (std::vector<int16_t>{1, 2, 3, 4}));
}
```

- [ ] **Step 2: Run it to verify it fails to compile (no `audio_chunker.h` yet)**

```bash
cmake -S spikes/livekit-cpp-spike -B spike-build -DLIVEKIT_SPIKE_BUILD_TESTS=ON
cmake --build spike-build --target audio_chunker_tests
```

Expected: FAIL — `audio_chunker.h: No such file or directory`.

- [ ] **Step 3: Write `spikes/livekit-cpp-spike/src/audio_chunker.h`**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace pcm::spike {

// Accumulates interleaved int16 PCM samples and yields fixed-size frames.
// Framework-free (no Qt, no LiveKit) so it can be unit-tested directly.
class AudioChunker {
public:
  AudioChunker(std::size_t samplesPerChannel, int channels);

  // Appends newSamples to the internal buffer and returns every complete
  // frame that can now be formed. Any leftover samples are kept for the
  // next call.
  [[nodiscard]] std::vector<std::vector<int16_t>> push(
      const std::vector<int16_t> &newSamples);

private:
  std::size_t mFrameSize;  // samplesPerChannel * channels
  std::deque<int16_t> mBuffer;
};

}  // namespace pcm::spike
```

- [ ] **Step 4: Write `spikes/livekit-cpp-spike/src/audio_chunker.cpp`**

```cpp
#include "audio_chunker.h"

namespace pcm::spike {

AudioChunker::AudioChunker(const std::size_t samplesPerChannel,
                           const int channels)
    : mFrameSize(samplesPerChannel * static_cast<std::size_t>(channels)) {}

std::vector<std::vector<int16_t>> AudioChunker::push(
    const std::vector<int16_t> &newSamples) {
  mBuffer.insert(mBuffer.end(), newSamples.begin(), newSamples.end());

  std::vector<std::vector<int16_t>> frames;
  while (mBuffer.size() >= mFrameSize) {
    std::vector<int16_t> frame(mBuffer.begin(), mBuffer.begin() + static_cast<std::ptrdiff_t>(mFrameSize));
    frames.push_back(std::move(frame));
    mBuffer.erase(mBuffer.begin(), mBuffer.begin() + static_cast<std::ptrdiff_t>(mFrameSize));
  }
  return frames;
}

}  // namespace pcm::spike
```

- [ ] **Step 5: Run the audio chunker test to verify it passes**

```bash
cmake --build spike-build --target audio_chunker_tests
./spike-build/test/audio_chunker_tests
```

Expected: all 5 cases PASS.

- [ ] **Step 6: Write the failing test for `videoFrameToLiveKitRGBA`**

```cpp
// spikes/livekit-cpp-spike/test/frame_convert_tests.cpp
#include <gtest/gtest.h>

#include <QImage>

#include "frame_convert.h"

using pcm::spike::videoFrameToLiveKitRGBA;

TEST(FrameConvertTest, ConvertsSolidRedImageToRGBABytes) {
  QImage image(4, 3, QImage::Format_RGB32);
  image.fill(QColor(255, 0, 0));

  const auto frame = videoFrameToLiveKitRGBA(image);

  EXPECT_EQ(frame.width(), 4);
  EXPECT_EQ(frame.height(), 3);
  ASSERT_EQ(frame.dataSize(), static_cast<std::size_t>(4 * 3 * 4));

  const auto *data = frame.data();
  for (int i = 0; i < 4 * 3; ++i) {
    EXPECT_EQ(data[i * 4 + 0], 255) << "pixel " << i << " R";
    EXPECT_EQ(data[i * 4 + 1], 0) << "pixel " << i << " G";
    EXPECT_EQ(data[i * 4 + 2], 0) << "pixel " << i << " B";
    EXPECT_EQ(data[i * 4 + 3], 255) << "pixel " << i << " A";
  }
}

TEST(FrameConvertTest, HandlesArgbSourceFormat) {
  QImage image(2, 2, QImage::Format_ARGB32);
  image.fill(QColor(0, 255, 0, 128));

  const auto frame = videoFrameToLiveKitRGBA(image);
  EXPECT_EQ(frame.width(), 2);
  EXPECT_EQ(frame.height(), 2);
  const auto *data = frame.data();
  EXPECT_EQ(data[0], 0);
  EXPECT_EQ(data[1], 255);
  EXPECT_EQ(data[2], 0);
}
```

This test does not construct a `QApplication` — `QImage` pixel manipulation
works without one, consistent with the repo's existing no-`QApplication`
test convention.

- [ ] **Step 7: Run it to verify it fails to compile**

```bash
cmake --build spike-build --target frame_convert_tests
```

Expected: FAIL — `frame_convert.h: No such file or directory`.

- [ ] **Step 8: Write `spikes/livekit-cpp-spike/src/frame_convert.h`**

```cpp
#pragma once

#include <QImage>

#include "livekit/livekit.h"

namespace pcm::spike {

// Converts a QImage (any Qt pixel format) to a livekit::VideoFrame in RGBA
// byte order, matching what QEventItem-style RGBA-capable VideoSources
// expect (see basic_room/capture_utils.cpp in the LiveKit examples: RGBA is
// directly supported, no I420 conversion required).
[[nodiscard]] livekit::VideoFrame videoFrameToLiveKitRGBA(const QImage &image);

}  // namespace pcm::spike
```

- [ ] **Step 9: Write `spikes/livekit-cpp-spike/src/frame_convert.cpp`**

```cpp
#include "frame_convert.h"

namespace pcm::spike {

livekit::VideoFrame videoFrameToLiveKitRGBA(const QImage &image) {
  const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);

  auto frame = livekit::VideoFrame::create(rgba.width(), rgba.height(),
                                           livekit::VideoBufferType::RGBA);

  std::uint8_t *dst = frame.data();
  const std::size_t dstStride = static_cast<std::size_t>(rgba.width()) * 4;
  for (int y = 0; y < rgba.height(); ++y) {
    std::memcpy(dst + static_cast<std::size_t>(y) * dstStride,
               rgba.constScanLine(y), dstStride);
  }
  return frame;
}

}  // namespace pcm::spike
```

Add `#include <cstring>` and `#include <cstdint>` to the top of this file
alongside the existing includes (needed for `std::memcpy`/`std::uint8_t`).

- [ ] **Step 10: Write `spikes/livekit-cpp-spike/test/CMakeLists.txt`**

```cmake
find_package(GTest CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_executable(audio_chunker_tests
  audio_chunker_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/audio_chunker.cpp
)
target_include_directories(audio_chunker_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(audio_chunker_tests PRIVATE GTest::gtest GTest::gtest_main)

add_executable(frame_convert_tests
  frame_convert_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/frame_convert.cpp
)
target_include_directories(frame_convert_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(frame_convert_tests PRIVATE
  GTest::gtest GTest::gtest_main
  LiveKit::livekit
  Qt6::Core
  Qt6::Gui
)
```

If `find_package(GTest CONFIG REQUIRED)` fails because GoogleTest isn't
available outside the main project's vcpkg-managed dependency tree, that is
expected — this spike's `CMakeLists.txt` doesn't use the main project's
vcpkg toolchain file. In that case, fetch it directly instead: replace the
`find_package(GTest ...)` line with:

```cmake
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)
```

and link against `gtest gtest_main` (the plain target names FetchContent
produces) instead of `GTest::gtest GTest::gtest_main`. Try `find_package`
first since it's faster; only fall back to `FetchContent` if it fails.

- [ ] **Step 11: Update `spikes/livekit-cpp-spike/CMakeLists.txt` to add the new sources to the main target**

```cmake
qt_add_executable(livekit_spike
  src/main.cpp
  src/audio_chunker.cpp
  src/frame_convert.cpp
)
```

- [ ] **Step 12: Run both tests to verify they pass**

```bash
cmake -S spikes/livekit-cpp-spike -B spike-build -DLIVEKIT_SPIKE_BUILD_TESTS=ON
cmake --build spike-build --parallel
./spike-build/test/audio_chunker_tests
./spike-build/test/frame_convert_tests
```

Expected: all cases in both suites PASS. Also re-run
`./spike-build/livekit_spike` from Task 1 to confirm the main executable
still builds and runs after adding the two new source files to its target.

- [ ] **Step 13: Commit**

```bash
git add spikes/livekit-cpp-spike/CMakeLists.txt \
        spikes/livekit-cpp-spike/src/audio_chunker.h \
        spikes/livekit-cpp-spike/src/audio_chunker.cpp \
        spikes/livekit-cpp-spike/src/frame_convert.h \
        spikes/livekit-cpp-spike/src/frame_convert.cpp \
        spikes/livekit-cpp-spike/test/CMakeLists.txt \
        spikes/livekit-cpp-spike/test/audio_chunker_tests.cpp \
        spikes/livekit-cpp-spike/test/frame_convert_tests.cpp
git commit -m "spike(livekit): add pure audio-chunking and video-frame-conversion utilities with tests"
```

---

### Task 3: Local capture adapters (camera + microphone) wired into LiveKit sources

**Files:**
- Create: `spikes/livekit-cpp-spike/src/video_capture_adapter.h`
- Create: `spikes/livekit-cpp-spike/src/video_capture_adapter.cpp`
- Create: `spikes/livekit-cpp-spike/src/audio_capture_adapter.h`
- Create: `spikes/livekit-cpp-spike/src/audio_capture_adapter.cpp`
- Create: `spikes/livekit-cpp-spike/src/spike_window.h`
- Create: `spikes/livekit-cpp-spike/src/spike_window.cpp`
- Modify: `spikes/livekit-cpp-spike/src/main.cpp` (replace body to construct `SpikeWindow` instead of the Task 1 smoke test)
- Modify: `spikes/livekit-cpp-spike/CMakeLists.txt:1-64` (add the four new `.cpp` files to `qt_add_executable`)

**Interfaces:**
- Consumes: `pcm::spike::videoFrameToLiveKitRGBA` and `pcm::spike::AudioChunker` from Task 2.
- Produces: `VideoCaptureAdapter` (owns a `std::shared_ptr<livekit::VideoSource>`, exposes `videoSource()`, `start(const QCameraDevice&)`, `stop()`, `switchDevice(const QCameraDevice&)`, and a `framesCaptured()` counter for Task 6's UI) and `AudioCaptureAdapter` (owns a `std::shared_ptr<livekit::AudioSource>`, exposes `audioSource()`, `start(const QAudioDevice&)`, `stop()`, `switchDevice(const QAudioDevice&)`, `framesCaptured()`) — both consumed by Task 4 (publishing) and Task 6 (device-switch UI, teardown).

- [ ] **Step 1: Write `spikes/livekit-cpp-spike/src/video_capture_adapter.h`**

```cpp
#pragma once

#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QObject>
#include <QVideoSink>

#include <atomic>
#include <memory>

#include "livekit/livekit.h"

// Drives a QCamera through Qt Multimedia and pushes every captured frame,
// converted to RGBA, into a livekit::VideoSource. Owns no LiveKit track —
// callers publish videoSource() themselves (see spike_window.cpp).
class VideoCaptureAdapter final : public QObject {
  Q_OBJECT

public:
  explicit VideoCaptureAdapter(QObject *parent = nullptr);
  ~VideoCaptureAdapter() override;

  [[nodiscard]] std::shared_ptr<livekit::VideoSource> videoSource() const { return mVideoSource; }
  [[nodiscard]] QVideoSink *previewSink() { return &mSink; }
  [[nodiscard]] int framesCaptured() const { return mFramesCaptured.load(); }

  // Starts (or restarts, if already running) capture on the given device.
  void start(const QCameraDevice &device);
  void stop();

signals:
  void frameCaptured();

private slots:
  void onVideoFrameChanged(const QVideoFrame &frame);

private:
  std::shared_ptr<livekit::VideoSource> mVideoSource;
  std::unique_ptr<QCamera> mCamera;
  QMediaCaptureSession mSession;
  QVideoSink mSink;
  std::atomic<int> mFramesCaptured{0};
};
```

- [ ] **Step 2: Write `spikes/livekit-cpp-spike/src/video_capture_adapter.cpp`**

```cpp
#include "video_capture_adapter.h"

#include <QMediaDevices>

#include <iostream>

#include "frame_convert.h"

namespace {
constexpr int kVideoWidth = 1280;
constexpr int kVideoHeight = 720;
}  // namespace

VideoCaptureAdapter::VideoCaptureAdapter(QObject *parent)
    : QObject(parent),
      mVideoSource(std::make_shared<livekit::VideoSource>(kVideoWidth, kVideoHeight)) {
  mSession.setVideoSink(&mSink);
  connect(&mSink, &QVideoSink::videoFrameChanged, this,
          &VideoCaptureAdapter::onVideoFrameChanged);
}

VideoCaptureAdapter::~VideoCaptureAdapter() { stop(); }

void VideoCaptureAdapter::start(const QCameraDevice &device) {
  stop();
  mCamera = std::make_unique<QCamera>(device, this);
  mSession.setCamera(mCamera.get());
  mCamera->start();
}

void VideoCaptureAdapter::stop() {
  if (mCamera) {
    mCamera->stop();
    mSession.setCamera(nullptr);
    mCamera.reset();
  }
}

void VideoCaptureAdapter::onVideoFrameChanged(const QVideoFrame &frame) {
  if (!frame.isValid()) {
    return;
  }
  const QImage image = frame.toImage();
  if (image.isNull()) {
    return;
  }

  const auto liveKitFrame = pcm::spike::videoFrameToLiveKitRGBA(image);
  try {
    mVideoSource->captureFrame(liveKitFrame, 0, livekit::VideoRotation::VIDEO_ROTATION_0);
    mFramesCaptured.fetch_add(1);
    emit frameCaptured();
  } catch (const std::exception &e) {
    std::cerr << "[video capture] captureFrame failed: " << e.what() << std::endl;
  }
}
```

- [ ] **Step 3: Write `spikes/livekit-cpp-spike/src/audio_capture_adapter.h`**

```cpp
#pragma once

#include <QAudioDevice>
#include <QAudioSource>
#include <QIODevice>
#include <QObject>

#include <atomic>
#include <memory>

#include "audio_chunker.h"
#include "livekit/livekit.h"

// Drives a QAudioSource through Qt Multimedia and pushes fixed-size PCM
// frames into a livekit::AudioSource. Owns no LiveKit track — callers
// publish audioSource() themselves (see spike_window.cpp).
class AudioCaptureAdapter final : public QObject {
  Q_OBJECT

public:
  static constexpr int kSampleRate = 48000;
  static constexpr int kChannels = 1;
  static constexpr int kFrameMs = 10;

  explicit AudioCaptureAdapter(QObject *parent = nullptr);
  ~AudioCaptureAdapter() override;

  [[nodiscard]] std::shared_ptr<livekit::AudioSource> audioSource() const { return mAudioSource; }
  [[nodiscard]] int framesCaptured() const { return mFramesCaptured.load(); }

  void start(const QAudioDevice &device);
  void stop();

signals:
  void frameCaptured();

private slots:
  void onReadyRead();

private:
  std::shared_ptr<livekit::AudioSource> mAudioSource;
  std::unique_ptr<QAudioSource> mSource;
  QIODevice *mIoDevice{nullptr};
  pcm::spike::AudioChunker mChunker;
  std::atomic<int> mFramesCaptured{0};
};
```

- [ ] **Step 4: Write `spikes/livekit-cpp-spike/src/audio_capture_adapter.cpp`**

```cpp
#include "audio_capture_adapter.h"

#include <iostream>

AudioCaptureAdapter::AudioCaptureAdapter(QObject *parent)
    : QObject(parent),
      mAudioSource(std::make_shared<livekit::AudioSource>(kSampleRate, kChannels, kFrameMs)),
      mChunker(static_cast<std::size_t>(kSampleRate * kFrameMs / 1000), kChannels) {}

AudioCaptureAdapter::~AudioCaptureAdapter() { stop(); }

void AudioCaptureAdapter::start(const QAudioDevice &device) {
  stop();

  QAudioFormat format;
  format.setSampleRate(kSampleRate);
  format.setChannelCount(kChannels);
  format.setSampleFormat(QAudioFormat::Int16);

  mSource = std::make_unique<QAudioSource>(device, format, this);
  mIoDevice = mSource->start();
  if (!mIoDevice) {
    std::cerr << "[audio capture] QAudioSource::start() returned null (error="
              << static_cast<int>(mSource->error()) << ")" << std::endl;
    return;
  }
  connect(mIoDevice, &QIODevice::readyRead, this, &AudioCaptureAdapter::onReadyRead);
}

void AudioCaptureAdapter::stop() {
  if (mSource) {
    mSource->stop();
    mIoDevice = nullptr;
    mSource.reset();
  }
}

void AudioCaptureAdapter::onReadyRead() {
  if (!mIoDevice) {
    return;
  }
  const QByteArray bytes = mIoDevice->readAll();
  if (bytes.isEmpty()) {
    return;
  }

  std::vector<int16_t> samples(static_cast<std::size_t>(bytes.size()) / sizeof(int16_t));
  std::memcpy(samples.data(), bytes.constData(), samples.size() * sizeof(int16_t));

  for (const auto &pcmFrame : mChunker.push(samples)) {
    auto liveKitFrame = livekit::AudioFrame::create(kSampleRate, kChannels, pcmFrame.size() / kChannels);
    auto &dst = liveKitFrame.data();
    std::copy(pcmFrame.begin(), pcmFrame.end(), dst.begin());

    try {
      mAudioSource->captureFrame(liveKitFrame);
      mFramesCaptured.fetch_add(1);
      emit frameCaptured();
    } catch (const std::exception &e) {
      std::cerr << "[audio capture] captureFrame failed: " << e.what() << std::endl;
    }
  }
}
```

Add `#include <cstring>` and `#include <cstdint>` alongside the existing
includes at the top of this file (needed for `std::memcpy`/`int16_t`).

- [ ] **Step 5: Write `spikes/livekit-cpp-spike/src/spike_window.h`**

```cpp
#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QVideoWidget>

#include <memory>

#include "audio_capture_adapter.h"
#include "video_capture_adapter.h"

// Minimal window for the LiveKit spike. This task only wires up local
// capture + preview; Task 4 adds room connection, Task 5 adds remote
// rendering, Task 6 adds device switching and teardown polish.
class SpikeWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit SpikeWindow(QWidget *parent = nullptr);

private:
  VideoCaptureAdapter mVideoCapture;
  AudioCaptureAdapter mAudioCapture;
  QVideoWidget *mLocalPreview{nullptr};
  QLabel *mStatusLabel{nullptr};

  void updateStatusLabel();
};
```

- [ ] **Step 6: Write `spikes/livekit-cpp-spike/src/spike_window.cpp`**

```cpp
#include "spike_window.h"

#include <QMediaDevices>
#include <QVBoxLayout>
#include <QWidget>

SpikeWindow::SpikeWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("LiveKit C++/Qt Spike");
  resize(960, 640);

  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);

  mLocalPreview = new QVideoWidget(central);
  mLocalPreview->setMinimumSize(640, 360);
  layout->addWidget(mLocalPreview);

  mStatusLabel = new QLabel(central);
  layout->addWidget(mStatusLabel);

  setCentralWidget(central);

  // Show the local capture in this window's own preview, independent of
  // what we push into LiveKit's VideoSource — proves capture works even
  // before Task 4 wires up a room connection.
  mLocalPreview->videoSink()->disconnect();
  connect(mVideoCapture.previewSink(), &QVideoSink::videoFrameChanged,
          mLocalPreview->videoSink(), &QVideoSink::setVideoFrame);

  connect(&mVideoCapture, &VideoCaptureAdapter::frameCaptured, this,
          &SpikeWindow::updateStatusLabel);
  connect(&mAudioCapture, &AudioCaptureAdapter::frameCaptured, this,
          &SpikeWindow::updateStatusLabel);

  const auto cameras = QMediaDevices::videoInputs();
  if (!cameras.isEmpty()) {
    mVideoCapture.start(cameras.first());
  } else {
    mStatusLabel->setText("No camera device found.");
  }

  const auto mics = QMediaDevices::audioInputs();
  if (!mics.isEmpty()) {
    mAudioCapture.start(mics.first());
  }

  updateStatusLabel();
}

void SpikeWindow::updateStatusLabel() {
  mStatusLabel->setText(QStringLiteral("Video frames captured: %1   Audio frames captured: %2")
                            .arg(mVideoCapture.framesCaptured())
                            .arg(mAudioCapture.framesCaptured()));
}
```

- [ ] **Step 7: Replace `spikes/livekit-cpp-spike/src/main.cpp`**

```cpp
// Throwaway spike for issue #77 (LiveKit C++/Qt capture-and-render proof).
// Not shipped, not part of the main app build.
#include <QApplication>

#include <iostream>

#include "livekit/livekit.h"
#include "spike_window.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  livekit::initialize(livekit::LogLevel::Info);
  std::cout << "LiveKit version: " << LIVEKIT_BUILD_VERSION_FULL << std::endl;

  SpikeWindow window;
  window.show();

  const int result = app.exec();

  livekit::shutdown();
  return result;
}
```

- [ ] **Step 8: Update `spikes/livekit-cpp-spike/CMakeLists.txt`'s executable sources**

```cmake
qt_add_executable(livekit_spike
  src/main.cpp
  src/audio_chunker.cpp
  src/frame_convert.cpp
  src/video_capture_adapter.cpp
  src/audio_capture_adapter.cpp
  src/spike_window.cpp
)
```

- [ ] **Step 9: Build and run**

```bash
cmake --build spike-build --parallel
./spike-build/livekit_spike
```

Expected: a window opens showing the local camera preview (or "No camera
device found" if this machine/CI runner has none — that's fine, it must
not crash), and the status label's frame counters increase over time,
proving `captureFrame()` is being called successfully for both audio and
video without throwing. Let it run for at least 30 seconds, then close the
window and confirm the process exits cleanly with no crash (teardown
polish is Task 6 — for now, just confirm no crash on close).

- [ ] **Step 10: Commit**

```bash
git add spikes/livekit-cpp-spike/CMakeLists.txt \
        spikes/livekit-cpp-spike/src/video_capture_adapter.h \
        spikes/livekit-cpp-spike/src/video_capture_adapter.cpp \
        spikes/livekit-cpp-spike/src/audio_capture_adapter.h \
        spikes/livekit-cpp-spike/src/audio_capture_adapter.cpp \
        spikes/livekit-cpp-spike/src/spike_window.h \
        spikes/livekit-cpp-spike/src/spike_window.cpp \
        spikes/livekit-cpp-spike/src/main.cpp
git commit -m "spike(livekit): wire QCamera/QAudioSource capture into LiveKit Video/AudioSource, local preview window"
```

---

### Task 4: Room connection and track publishing

**Files:**
- Modify: `spikes/livekit-cpp-spike/src/spike_window.h:1-25` (add room/delegate members, Join/Leave)
- Modify: `spikes/livekit-cpp-spike/src/spike_window.cpp` (add connection logic)

**Interfaces:**
- Consumes: `VideoCaptureAdapter::videoSource()`, `AudioCaptureAdapter::audioSource()` from Task 3.
- Produces: `SpikeWindow`'s room/track state, consumed by Task 5 (remote track subscription happens through the same `RoomDelegate`) and Task 6 (Leave button unpublishes and disconnects).

- [ ] **Step 1: Add room/delegate members and the Join/Leave button to `spike_window.h`**

```cpp
#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVideoWidget>

#include <memory>

#include "audio_capture_adapter.h"
#include "livekit/livekit.h"
#include "video_capture_adapter.h"

class SpikeWindow final : public QMainWindow, public livekit::RoomDelegate {
  Q_OBJECT

public:
  explicit SpikeWindow(QWidget *parent = nullptr);
  ~SpikeWindow() override;

  // livekit::RoomDelegate overrides
  void onParticipantConnected(livekit::Room &room,
                              const livekit::ParticipantConnectedEvent &ev) override;
  void onTrackSubscribed(livekit::Room &room,
                         const livekit::TrackSubscribedEvent &ev) override;

private slots:
  void onJoinClicked();
  void onLeaveClicked();

private:
  VideoCaptureAdapter mVideoCapture;
  AudioCaptureAdapter mAudioCapture;
  QVideoWidget *mLocalPreview{nullptr};
  QLabel *mStatusLabel{nullptr};
  QLabel *mConnectionLabel{nullptr};
  QPushButton *mJoinButton{nullptr};
  QPushButton *mLeaveButton{nullptr};

  std::unique_ptr<livekit::Room> mRoom;
  std::shared_ptr<livekit::LocalAudioTrack> mAudioTrack;
  std::shared_ptr<livekit::LocalVideoTrack> mVideoTrack;

  void updateStatusLabel();
  void setConnectionState(const QString &text);
  void publishTracks();
  void unpublishTracks();
};
```

- [ ] **Step 2: Implement Join/Leave and the delegate callbacks in `spike_window.cpp`**

Add these includes near the top, alongside the existing ones:

```cpp
#include <QHBoxLayout>
#include <QMetaObject>

#include <cstdlib>
#include <iostream>
```

Add the Join/Leave buttons and connection-state label to the constructor,
right after `mStatusLabel` is added to `layout`:

```cpp
  mConnectionLabel = new QLabel("Not connected.", central);
  layout->addWidget(mConnectionLabel);

  auto *buttonRow = new QWidget(central);
  auto *buttonLayout = new QHBoxLayout(buttonRow);
  mJoinButton = new QPushButton("Join", buttonRow);
  mLeaveButton = new QPushButton("Leave", buttonRow);
  mLeaveButton->setEnabled(false);
  buttonLayout->addWidget(mJoinButton);
  buttonLayout->addWidget(mLeaveButton);
  layout->addWidget(buttonRow);

  connect(mJoinButton, &QPushButton::clicked, this, &SpikeWindow::onJoinClicked);
  connect(mLeaveButton, &QPushButton::clicked, this, &SpikeWindow::onLeaveClicked);
```

Append the rest of the implementation at the end of `spike_window.cpp`:

```cpp
SpikeWindow::~SpikeWindow() { onLeaveClicked(); }

void SpikeWindow::setConnectionState(const QString &text) {
  mConnectionLabel->setText(text);
}

void SpikeWindow::onJoinClicked() {
  const char *url = std::getenv("LIVEKIT_URL");
  const char *token = std::getenv("LIVEKIT_TOKEN");
  if (!url || !token) {
    setConnectionState("LIVEKIT_URL / LIVEKIT_TOKEN not set — cannot join.");
    return;
  }

  mRoom = std::make_unique<livekit::Room>();
  mRoom->setDelegate(this);

  livekit::RoomOptions options;
  options.auto_subscribe = true;
  options.dynacast = false;

  setConnectionState("Connecting...");
  const bool connected = mRoom->connect(url, token, options);
  if (!connected) {
    setConnectionState("Failed to connect.");
    mRoom->setDelegate(nullptr);
    mRoom.reset();
    return;
  }

  setConnectionState("Connected.");
  mJoinButton->setEnabled(false);
  mLeaveButton->setEnabled(true);
  publishTracks();
}

void SpikeWindow::publishTracks() {
  auto lp = mRoom->localParticipant().lock();
  if (!lp) {
    std::cerr << "[room] local participant unavailable, cannot publish" << std::endl;
    return;
  }

  mAudioTrack = livekit::LocalAudioTrack::createLocalAudioTrack("mic", mAudioCapture.audioSource());
  livekit::TrackPublishOptions audioOpts;
  audioOpts.source = livekit::TrackSource::SOURCE_MICROPHONE;
  audioOpts.dtx = false;
  audioOpts.simulcast = false;
  try {
    lp->publishTrack(mAudioTrack, audioOpts);
  } catch (const std::exception &e) {
    std::cerr << "[room] failed to publish audio track: " << e.what() << std::endl;
  }

  mVideoTrack = livekit::LocalVideoTrack::createLocalVideoTrack("cam", mVideoCapture.videoSource());
  livekit::TrackPublishOptions videoOpts;
  videoOpts.source = livekit::TrackSource::SOURCE_CAMERA;
  videoOpts.dtx = false;
  videoOpts.simulcast = true;
  try {
    lp->publishTrack(mVideoTrack, videoOpts);
  } catch (const std::exception &e) {
    std::cerr << "[room] failed to publish video track: " << e.what() << std::endl;
  }
}

void SpikeWindow::unpublishTracks() {
  if (mRoom) {
    if (auto lp = mRoom->localParticipant().lock()) {
      if (mAudioTrack && mAudioTrack->publication()) {
        lp->unpublishTrack(mAudioTrack->publication()->sid());
      }
      if (mVideoTrack && mVideoTrack->publication()) {
        lp->unpublishTrack(mVideoTrack->publication()->sid());
      }
    }
  }
  mAudioTrack.reset();
  mVideoTrack.reset();
}

void SpikeWindow::onLeaveClicked() {
  if (!mRoom) {
    return;
  }
  unpublishTracks();
  mRoom->setDelegate(nullptr);
  mRoom.reset();

  mJoinButton->setEnabled(true);
  mLeaveButton->setEnabled(false);
  setConnectionState("Not connected.");
}

void SpikeWindow::onParticipantConnected(livekit::Room & /*room*/,
                                         const livekit::ParticipantConnectedEvent &ev) {
  const QString identity = ev.participant ? QString::fromStdString(ev.participant->identity())
                                          : QStringLiteral("<unknown>");
  QMetaObject::invokeMethod(
      this,
      [this, identity]() {
        setConnectionState(QStringLiteral("Connected. Participant joined: %1").arg(identity));
      },
      Qt::QueuedConnection);
}

void SpikeWindow::onTrackSubscribed(livekit::Room & /*room*/,
                                    const livekit::TrackSubscribedEvent & /*ev*/) {
  // Task 5 attaches the remote video/audio renderer here.
}
```

`RoomDelegate` callbacks fire on a LiveKit-internal thread, not the Qt UI
thread — every callback that touches a widget must go through
`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`, exactly as done
above for `onParticipantConnected`. Task 5 must follow the same pattern.

- [ ] **Step 3: Build**

```bash
cmake --build spike-build --parallel
```

Expected: clean build. `SpikeWindow` now inherits both `QMainWindow` and
`livekit::RoomDelegate` — if this produces a "cannot inherit from both
QObject and a non-QObject base with conflicting virtual tables" or similar
diamond/MOC error, switch to composition instead: make a private nested
`Delegate : public livekit::RoomDelegate` class that holds a
`SpikeWindow *mOwner` and forwards callbacks via a plain method call
(`mOwner->handleParticipantConnected(ev)`), and give `mRoom->setDelegate()`
that nested instance instead of `this`. Only do this if the direct
multiple-inheritance approach above actually fails to compile — try the
simpler direct-inheritance version first.

- [ ] **Step 4: Manual check (works even without real LiveKit credentials)**

```bash
./spike-build/livekit_spike
```

Click Join with `LIVEKIT_URL`/`LIVEKIT_TOKEN` unset: expect the label to
read "LIVEKIT_URL / LIVEKIT_TOKEN not set — cannot join." and no crash.
This is the only part of Task 4 testable without real credentials — actual
connect/publish only becomes testable once the user supplies
`LIVEKIT_URL`/`LIVEKIT_TOKEN` for their LiveKit Cloud staging project
(roadmap §5.3 point 6, tracked in the README checklist).

- [ ] **Step 5: Commit**

```bash
git add spikes/livekit-cpp-spike/src/spike_window.h spikes/livekit-cpp-spike/src/spike_window.cpp
git commit -m "spike(livekit): add Room connect/publish and Join/Leave UI"
```

---

### Task 5: Remote video rendering and remote audio playback

**Files:**
- Create: `spikes/livekit-cpp-spike/src/remote_video_renderer.h`
- Create: `spikes/livekit-cpp-spike/src/remote_video_renderer.cpp`
- Create: `spikes/livekit-cpp-spike/src/remote_audio_player.h`
- Create: `spikes/livekit-cpp-spike/src/remote_audio_player.cpp`
- Modify: `spikes/livekit-cpp-spike/src/spike_window.h` (own a `RemoteVideoRenderer`/`RemoteAudioPlayer`, wire into `onTrackSubscribed`)
- Modify: `spikes/livekit-cpp-spike/src/spike_window.cpp`
- Modify: `spikes/livekit-cpp-spike/CMakeLists.txt` (add the two new `.cpp` files)

**Interfaces:**
- Consumes: `livekit::TrackSubscribedEvent` from Task 4's `onTrackSubscribed`.
- Produces: `RemoteVideoRenderer` (a `QOpenGLWidget`, `attachTrack(std::shared_ptr<livekit::Track>)`, `detach()`) and `RemoteAudioPlayer` (`attachTrack(...)`, `detach()`) — consumed by Task 6 (teardown must call `detach()` on both).

- [ ] **Step 1: Write `spikes/livekit-cpp-spike/src/remote_video_renderer.h`**

```cpp
#pragma once

#include <QImage>
#include <QMutex>
#include <QOpenGLWidget>

#include <atomic>
#include <memory>
#include <thread>

#include "livekit/livekit.h"

// Renders a subscribed remote video track. Runs its own reader thread that
// blocks on livekit::VideoStream::read() (the SDK's frame-delivery API is
// pull-based, not callback-based) and marshals decoded QImages to the Qt
// UI thread for painting.
class RemoteVideoRenderer final : public QOpenGLWidget {
  Q_OBJECT

public:
  explicit RemoteVideoRenderer(QWidget *parent = nullptr);
  ~RemoteVideoRenderer() override;

  void attachTrack(const std::shared_ptr<livekit::Track> &track);
  void detach();

protected:
  void paintGL() override;

private:
  std::shared_ptr<livekit::VideoStream> mStream;
  std::thread mReaderThread;
  std::atomic<bool> mRunning{false};

  QMutex mFrameMutex;
  QImage mLatestFrame;

  void readerLoop();
  void setLatestFrame(const QImage &image);
};
```

- [ ] **Step 2: Write `spikes/livekit-cpp-spike/src/remote_video_renderer.cpp`**

```cpp
#include "remote_video_renderer.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QPainter>

#include <iostream>

RemoteVideoRenderer::RemoteVideoRenderer(QWidget *parent) : QOpenGLWidget(parent) {}

RemoteVideoRenderer::~RemoteVideoRenderer() { detach(); }

void RemoteVideoRenderer::attachTrack(const std::shared_ptr<livekit::Track> &track) {
  detach();
  if (!track) {
    return;
  }

  livekit::VideoStream::Options opts;
  opts.format = livekit::VideoBufferType::RGBA;
  mStream = livekit::VideoStream::fromTrack(track, opts);
  if (!mStream) {
    std::cerr << "[remote video] VideoStream::fromTrack failed" << std::endl;
    return;
  }

  mRunning.store(true);
  mReaderThread = std::thread(&RemoteVideoRenderer::readerLoop, this);
}

void RemoteVideoRenderer::detach() {
  mRunning.store(false);
  if (mReaderThread.joinable()) {
    mReaderThread.join();
  }
  mStream.reset();
}

void RemoteVideoRenderer::readerLoop() {
  while (mRunning.load()) {
    livekit::VideoFrameEvent vfe;
    if (!mStream->read(vfe)) {
      break;  // EOS / stream closed
    }

    livekit::VideoFrame &frame = vfe.frame;
    if (frame.type() != livekit::VideoBufferType::RGBA) {
      try {
        frame = frame.convert(livekit::VideoBufferType::RGBA, false);
      } catch (const std::exception &e) {
        std::cerr << "[remote video] convert to RGBA failed: " << e.what() << std::endl;
        continue;
      }
    }

    QImage image(frame.data(), frame.width(), frame.height(), QImage::Format_RGBA8888);
    setLatestFrame(image.copy());  // deep copy: frame.data() is only valid for this iteration
  }
}

void RemoteVideoRenderer::setLatestFrame(const QImage &image) {
  {
    QMutexLocker locker(&mFrameMutex);
    mLatestFrame = image;
  }
  QMetaObject::invokeMethod(this, QOverload<>::of(&QOpenGLWidget::update), Qt::QueuedConnection);
}

void RemoteVideoRenderer::paintGL() {
  QImage frame;
  {
    QMutexLocker locker(&mFrameMutex);
    frame = mLatestFrame;
  }
  if (frame.isNull()) {
    return;
  }

  QPainter painter(this);
  const QImage scaled = frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  const QPoint topLeft((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
  painter.drawImage(topLeft, scaled);
}
```

`QPainter` inside `paintGL()` is the simplest correct way to get a resized,
aspect-ratio-preserving image onto a `QOpenGLWidget` surface — it uses Qt's
own raster-to-GL path. This satisfies proof point 3 (native render with
correct resize) without hand-writing GL texture upload code, which would
be a large increase in scope for a spike whose job is to prove the
capture/publish/subscribe/render pipeline works end-to-end, not to
hand-optimize GPU upload.

- [ ] **Step 3: Write `spikes/livekit-cpp-spike/src/remote_audio_player.h`**

```cpp
#pragma once

#include <QAudioDevice>
#include <QAudioSink>
#include <QIODevice>
#include <QObject>

#include <atomic>
#include <memory>
#include <thread>

#include "livekit/livekit.h"

// Plays a subscribed remote audio track through a QAudioSink. Runs its own
// reader thread blocking on livekit::AudioStream::read() (pull-based, same
// as RemoteVideoRenderer) and writes PCM into the sink's QIODevice.
class RemoteAudioPlayer final : public QObject {
  Q_OBJECT

public:
  explicit RemoteAudioPlayer(QObject *parent = nullptr);
  ~RemoteAudioPlayer() override;

  void attachTrack(const std::shared_ptr<livekit::Track> &track, const QAudioDevice &outputDevice);
  void detach();

private:
  std::shared_ptr<livekit::AudioStream> mStream;
  std::unique_ptr<QAudioSink> mSink;
  QIODevice *mSinkDevice{nullptr};
  std::thread mReaderThread;
  std::atomic<bool> mRunning{false};

  void readerLoop();
};
```

- [ ] **Step 4: Write `spikes/livekit-cpp-spike/src/remote_audio_player.cpp`**

```cpp
#include "remote_audio_player.h"

#include <QByteArray>

#include <iostream>

RemoteAudioPlayer::RemoteAudioPlayer(QObject *parent) : QObject(parent) {}

RemoteAudioPlayer::~RemoteAudioPlayer() { detach(); }

void RemoteAudioPlayer::attachTrack(const std::shared_ptr<livekit::Track> &track,
                                    const QAudioDevice &outputDevice) {
  detach();
  if (!track) {
    return;
  }

  livekit::AudioStream::Options opts;
  mStream = livekit::AudioStream::fromTrack(track, opts);
  if (!mStream) {
    std::cerr << "[remote audio] AudioStream::fromTrack failed" << std::endl;
    return;
  }

  QAudioFormat format;
  format.setSampleRate(48000);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);

  mSink = std::make_unique<QAudioSink>(outputDevice, format, this);
  mSinkDevice = mSink->start();
  if (!mSinkDevice) {
    std::cerr << "[remote audio] QAudioSink::start() returned null" << std::endl;
    return;
  }

  mRunning.store(true);
  mReaderThread = std::thread(&RemoteAudioPlayer::readerLoop, this);
}

void RemoteAudioPlayer::detach() {
  mRunning.store(false);
  if (mReaderThread.joinable()) {
    mReaderThread.join();
  }
  if (mSink) {
    mSink->stop();
    mSink.reset();
  }
  mSinkDevice = nullptr;
  mStream.reset();
}

void RemoteAudioPlayer::readerLoop() {
  while (mRunning.load()) {
    livekit::AudioFrameEvent afe;
    if (!mStream->read(afe)) {
      break;
    }
    const auto &pcm = afe.frame.data();
    if (pcm.empty() || !mSinkDevice) {
      continue;
    }
    const QByteArray bytes(reinterpret_cast<const char *>(pcm.data()),
                           static_cast<qsizetype>(pcm.size() * sizeof(int16_t)));
    mSinkDevice->write(bytes);
  }
}
```

Add `#include <cstdint>` alongside the existing includes for `int16_t`.

- [ ] **Step 5: Wire both into `SpikeWindow`**

In `spike_window.h`, add:

```cpp
#include "remote_audio_player.h"
#include "remote_video_renderer.h"
```

and two members:

```cpp
  RemoteVideoRenderer *mRemoteVideo{nullptr};
  RemoteAudioPlayer mRemoteAudio;
```

In `spike_window.cpp`'s constructor, add the renderer to the layout (right
after `mLocalPreview`):

```cpp
  mRemoteVideo = new RemoteVideoRenderer(central);
  mRemoteVideo->setMinimumSize(640, 360);
  layout->addWidget(mRemoteVideo);
```

Replace the `onTrackSubscribed` body (currently a comment placeholder from
Task 4) with:

```cpp
void SpikeWindow::onTrackSubscribed(livekit::Room & /*room*/,
                                    const livekit::TrackSubscribedEvent &ev) {
  if (!ev.track) {
    return;
  }
  const auto kind = ev.track->kind();
  auto track = ev.track;

  QMetaObject::invokeMethod(
      this,
      [this, track, kind]() {
        if (kind == livekit::TrackKind::KIND_VIDEO) {
          mRemoteVideo->attachTrack(track);
          setConnectionState("Connected. Receiving remote video.");
        } else if (kind == livekit::TrackKind::KIND_AUDIO) {
          const auto outputs = QMediaDevices::audioOutputs();
          if (!outputs.isEmpty()) {
            mRemoteAudio.attachTrack(track, outputs.first());
          }
        }
      },
      Qt::QueuedConnection);
}
```

Add `#include <QMediaDevices>` to `spike_window.cpp` if it isn't already
there from Task 3.

- [ ] **Step 6: Update `spikes/livekit-cpp-spike/CMakeLists.txt`**

```cmake
qt_add_executable(livekit_spike
  src/main.cpp
  src/audio_chunker.cpp
  src/frame_convert.cpp
  src/video_capture_adapter.cpp
  src/audio_capture_adapter.cpp
  src/remote_video_renderer.cpp
  src/remote_audio_player.cpp
  src/spike_window.cpp
)
```

- [ ] **Step 7: Build**

```bash
cmake --build spike-build --parallel
```

Expected: clean build. Run `./spike-build/livekit_spike` once more and
confirm the window now shows both a local preview and an (empty, black)
remote video area with no crash — actually receiving a remote frame needs
a second participant, which is out of scope for this session (tracked in
the README checklist, point 6).

- [ ] **Step 8: Commit**

```bash
git add spikes/livekit-cpp-spike/CMakeLists.txt \
        spikes/livekit-cpp-spike/src/remote_video_renderer.h \
        spikes/livekit-cpp-spike/src/remote_video_renderer.cpp \
        spikes/livekit-cpp-spike/src/remote_audio_player.h \
        spikes/livekit-cpp-spike/src/remote_audio_player.cpp \
        spikes/livekit-cpp-spike/src/spike_window.h \
        spikes/livekit-cpp-spike/src/spike_window.cpp
git commit -m "spike(livekit): render subscribed remote video via QOpenGLWidget, play remote audio via QAudioSink"
```

---

### Task 6: Device switching, clean teardown, and CI integration

**Files:**
- Modify: `spikes/livekit-cpp-spike/src/spike_window.h` (add device combo boxes)
- Modify: `spikes/livekit-cpp-spike/src/spike_window.cpp`
- Modify: `.github/workflows/cmake-multi-platform.yml:244-246` (insert the new steps right after the existing `Build` step)

**Interfaces:**
- Consumes: `VideoCaptureAdapter::start()`/`stop()`, `AudioCaptureAdapter::start()`/`stop()` (Task 3), `RemoteVideoRenderer::detach()`, `RemoteAudioPlayer::detach()` (Task 5), `SpikeWindow::onLeaveClicked()` (Task 4).
- Produces: nothing consumed by later tasks — this is the plan's final task.

- [ ] **Step 1: Add device combo boxes to `spike_window.h`**

```cpp
#include <QComboBox>
```

```cpp
  QComboBox *mCameraCombo{nullptr};
  QComboBox *mMicCombo{nullptr};
  QComboBox *mSpeakerCombo{nullptr};
```

- [ ] **Step 2: Build the combo boxes and wire live switching in `spike_window.cpp`**

Add `#include <QComboBox>` if not already present via the header. In the
constructor, right after the Join/Leave `buttonRow` is added to `layout`,
add:

```cpp
  auto *deviceRow = new QWidget(central);
  auto *deviceLayout = new QHBoxLayout(deviceRow);

  mCameraCombo = new QComboBox(deviceRow);
  for (const auto &device : QMediaDevices::videoInputs()) {
    mCameraCombo->addItem(device.description(), QVariant::fromValue(device));
  }
  deviceLayout->addWidget(mCameraCombo);

  mMicCombo = new QComboBox(deviceRow);
  for (const auto &device : QMediaDevices::audioInputs()) {
    mMicCombo->addItem(device.description(), QVariant::fromValue(device));
  }
  deviceLayout->addWidget(mMicCombo);

  mSpeakerCombo = new QComboBox(deviceRow);
  for (const auto &device : QMediaDevices::audioOutputs()) {
    mSpeakerCombo->addItem(device.description(), QVariant::fromValue(device));
  }
  deviceLayout->addWidget(mSpeakerCombo);

  layout->addWidget(deviceRow);

  connect(mCameraCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index < 0) return;
    mVideoCapture.start(mCameraCombo->itemData(index).value<QCameraDevice>());
  });
  connect(mMicCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
    if (index < 0) return;
    mAudioCapture.start(mMicCombo->itemData(index).value<QAudioDevice>());
  });
```

`QComboBox::addItem` with a `QVariant::fromValue(...)` payload requires
`QCameraDevice` and `QAudioDevice` to be registered as Qt metatypes — Qt
Multimedia registers both automatically when `<QCameraDevice>`/
`<QAudioDevice>` are included, so no extra `Q_DECLARE_METATYPE` is needed;
if the build reports otherwise, add
`Q_DECLARE_METATYPE(QCameraDevice)`/`Q_DECLARE_METATYPE(QAudioDevice)`
right after the includes at the top of `spike_window.cpp`.

The speaker combo doesn't need a live-switch handler yet: `RemoteAudioPlayer`
only picks an output device when a remote audio track is attached
(Task 5's `onTrackSubscribed`). Change that call from
`outputs.first()` to `mSpeakerCombo->currentData().value<QAudioDevice>()`
so a manually chosen speaker is honored on the next attach — replace the
Task 5 `onTrackSubscribed` body's audio branch:

```cpp
        } else if (kind == livekit::TrackKind::KIND_AUDIO) {
          const auto selected = mSpeakerCombo->currentData();
          const auto outputs = QMediaDevices::audioOutputs();
          const auto device = selected.isValid() ? selected.value<QAudioDevice>()
                                                  : (outputs.isEmpty() ? QAudioDevice() : outputs.first());
          mRemoteAudio.attachTrack(track, device);
        }
```

- [ ] **Step 3: Complete teardown in `onLeaveClicked` and the destructor**

Replace `SpikeWindow::onLeaveClicked()`'s body (from Task 4) with the full
teardown sequence — stop remote playback/render, unpublish, disconnect,
then stop local capture:

```cpp
void SpikeWindow::onLeaveClicked() {
  mRemoteVideo->detach();
  mRemoteAudio.detach();

  if (mRoom) {
    unpublishTracks();
    mRoom->setDelegate(nullptr);
    mRoom.reset();
  }

  mJoinButton->setEnabled(true);
  mLeaveButton->setEnabled(false);
  setConnectionState("Not connected.");
}
```

Update the destructor to also stop local capture (device release, proof
point 5's "no leaked device handle" applies on app exit too, not just on
Leave):

```cpp
SpikeWindow::~SpikeWindow() {
  onLeaveClicked();
  mVideoCapture.stop();
  mAudioCapture.stop();
}
```

- [ ] **Step 4: Build and run the full teardown path**

```bash
cmake --build spike-build --parallel
./spike-build/livekit_spike
```

Manually: switch the camera and mic combo box selections a few times while
the status label's frame counters are visible — confirm they keep
incrementing after each switch (proof point 4, locally verifiable now).
Close the window — confirm no crash, and check with the OS (e.g.
`ls /dev/video*` usage via `fuser` on Linux, or just the camera-in-use LED)
that no camera/mic handle is left open after exit.

- [ ] **Step 5: Add the CI steps to `.github/workflows/cmake-multi-platform.yml`**

Insert immediately after the existing `- name: Build` step (currently at
line 244-245, `run: cmake --build build-release --parallel`) and before
`- name: Install bundle`:

```yaml
      - name: Configure LiveKit spike
        continue-on-error: true
        run: cmake -S spikes/livekit-cpp-spike -B spike-build

      - name: Build LiveKit spike
        continue-on-error: true
        run: cmake --build spike-build --parallel
```

Both steps reuse the job's existing checkout and the Qt environment
variables already exported earlier in the same job (`Qt6_DIR`,
`CMAKE_PREFIX_PATH`) — no new Qt-install step is added, and the existing
Qt-install steps that the main app's build depends on are left completely
untouched. If `Qt6MultimediaConfig.cmake`/`Qt6OpenGLWidgetsConfig.cmake`
aren't found because this environment's Qt installation doesn't include
those modules, `Configure LiveKit spike` goes red — `continue-on-error:
true` means this does not fail the job or block `Install bundle`/packaging/
`publish` below it. This is a known, acceptable gap for this round: note
it in the spike's README as a follow-up (see Step 6) rather than
attempting to guess the right Qt installer module IDs for three different
install mechanisms (the Linux official-installer script, `aqt`, and the
shared Windows/macOS `install-qt-action`) without being able to verify the
result in this environment.

- [ ] **Step 6: Add the known-gap note to the README's checklist**

Append to `spikes/livekit-cpp-spike/README.md`, right before the "Decision
gate" section:

```markdown
## Known gap: Qt module availability in CI

The CI steps above reuse the job's existing Qt installation rather than
adding a new install step, to avoid risking the main app's Windows/macOS
Qt setup with an unverified module list. If `Configure LiveKit spike`
fails with a `Could not find a package configuration file provided by
"Qt6Multimedia"` (or `Qt6OpenGLWidgets`) error on some OS, that Qt
installation is missing the module — extend that OS's existing Qt-install
step in `cmake-multi-platform.yml` (the `jurplel/install-qt-action` step
for Windows/macOS takes a `modules:` input; the Linux official-installer
step needs the matching `.addons.qtmultimedia` component id for Qt
6.10.2) and re-run CI to confirm.
```

- [ ] **Step 7: Commit**

```bash
git add spikes/livekit-cpp-spike/src/spike_window.h \
        spikes/livekit-cpp-spike/src/spike_window.cpp \
        spikes/livekit-cpp-spike/README.md \
        .github/workflows/cmake-multi-platform.yml
git commit -m "spike(livekit): device switching, full teardown on Leave/close, non-blocking 3-OS CI step"
```

---
