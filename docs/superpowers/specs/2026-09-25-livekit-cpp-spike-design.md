# LiveKit C++/Qt Capture-and-Render Spike Design

## Scope

Issue #77 (`[P1-B.0] Native LiveKit C++/Qt capture-and-render spike`), gating
issue #76 (P1-B tracking). Per `docs/video-roadmap.en.md` §5.3, this spike
must prove — before any product work (#78 token backend, #79 domain layer,
#80 call UI) begins — that the LiveKit C++ SDK can drive a native Qt
capture/render pipeline on Linux, Windows, and macOS. P1-B proceeds only if
all six proof points below are reproducible.

This is deliberately throwaway/isolated exploratory code, not a feature
branch. It is explicitly *not* attempting: a token backend (#78), the
`MeetingProvider`/`VideoProvider` domain layer (#79), or product call UI
(#80). A manually generated LiveKit token (via the LiveKit CLI or dashboard)
is used to test room connection instead of a real backend.

## Six proof points (from roadmap §5.3)

1. Building a fixed LiveKit C++ SDK version alongside the current CMake/vcpkg
   environment, on Linux, Windows, and macOS.
2. Camera + microphone capture via Qt Multimedia, without ownership/lifetime
   leaks.
3. Local preview + remote video rendered natively (`QOpenGLWidget`) with
   correct resize.
4. Switching camera/microphone/audio output mid-call.
5. Clean leave/destruction: stop tracks, unsubscribe callbacks, release
   devices, return to the UI thread.
6. A real two-machine call over staging LiveKit Cloud, both on an open
   network and one requiring TURN/TLS.

**Decision gate:** P1-B proceeds only if all six are reproducible and SDK
artifacts can be packaged for Linux/Windows/macOS. A failed spike is an
explicit architecture decision (fix the adapter / narrow OS support /
different SDK) — never a silent fallback to a WebView.

**Split of responsibility for this round:** this working session builds the
spike code and proves point 1 in CI across all three OSes (see CI section).
Points 2–6 require real camera/microphone hardware, and point 6 requires a
second machine and staging LiveKit Cloud credentials — GitHub Actions
runners have neither. The user will run the manual verification checklist
(see Verification section) on real Linux/Windows/macOS machines themselves.

## Isolation

Code lives entirely under `spikes/livekit-cpp-spike/`, as its own CMake
subdirectory with its own executable target (`livekit_spike`). It is not
`add_subdirectory`'d from the main app's target tree and does not link into
`PsyClientManager_app`. It does not touch `vcpkg.json` — the LiveKit C++ SDK
is not available via vcpkg.

`spikes/livekit-cpp-spike/cmake/LiveKitSDK.cmake` is copied from
`livekit-examples/cpp-example-collection` (Apache 2.0) and downloads a
pinned prebuilt SDK release at CMake configure time — the same pinning
discipline the project already uses for vcpkg (`VCPKG_COMMIT` in the CI
workflow). The spike's own `CMakeLists.txt` calls
`find_package(Qt6 COMPONENTS Core Gui Widgets Multimedia OpenGLWidgets
REQUIRED)` — `Multimedia` and `OpenGLWidgets` are new Qt modules, used only
within the spike, not added to the main app's dependencies.

## Architecture: capture and render (Qt Multimedia only)

Per explicit decision: capture and render go entirely through Qt
(`QCamera`/`QAudioSource`/`QOpenGLWidget`), with **no** platform-native
fallback (V4L2/Media Foundation/AVFoundation) built into the code. If Qt
Multimedia cannot do the job on some OS, that is the spike's decision-gate
failure to report, not something to route around with per-platform code.

- **`VideoCaptureAdapter`** — wraps `QMediaCaptureSession` + `QCamera`.
  Converts each `QVideoFrame` to I420 (the format LiveKit's `VideoFrame`
  expects) and publishes it via `livekit::VideoSource::captureFrame()`.
- **`AudioCaptureAdapter`** — wraps `QAudioSource`. Converts each PCM buffer
  to the frame format `livekit::AudioSource` expects.
- **`RemoteVideoRenderer`** — a `QOpenGLWidget` subclass. Receives raw
  remote frames via the SDK's `setOnVideoFrameCallback()`, uploads them as a
  GL texture, and repaints with correct behavior on widget resize.
- **`SpikeWindow`** (`QMainWindow`) — hosts:
  - A local preview (the same captured stream, shown via a `QVideoWidget` or
    a second instance of the render path).
  - The `RemoteVideoRenderer`.
  - Combo boxes for camera/microphone/speaker selection, wired to the
    adapters so switching is live (proof point 4).
  - Join/Leave buttons and a plain-text connection-state label — a minimal
    ad hoc indicator, not the full `VideoSession` state machine from roadmap
    §5.5 (that's #79's job).
  - Clean teardown on window close and on Leave: stop tracks, unsubscribe
    SDK callbacks, release camera/mic/speaker, return control to the UI
    thread (proof point 5).

## Credentials

`LIVEKIT_URL` and `LIVEKIT_TOKEN` are read from environment variables at
process start. The token is generated manually (LiveKit CLI or the LiveKit
Cloud dashboard) against the user's existing staging project — no token
backend exists yet (#78). Nothing is hardcoded or committed; the spike
executable refuses to attempt a room connection (but still runs for local
capture/preview testing) if either variable is unset.

## CI integration

Added to `.github/workflows/cmake-multi-platform.yml`, reusing the existing
3-OS matrix (`ubuntu-latest`, `macos-latest`, `windows-2022`). A new step
runs immediately after the existing `Build` step:

```yaml
- name: Configure LiveKit spike
  continue-on-error: true
  run: cmake -S spikes/livekit-cpp-spike -B spike-build

- name: Build LiveKit spike
  continue-on-error: true
  run: cmake --build spike-build --parallel
```

Both steps use `continue-on-error: true` — a spike build failure on any OS
shows red in the job's step list (visible, not silently ignored) but does
not fail the job, and does not block packaging, AppImage/DMG/installer
generation, or the `publish` job. The spike has its own build directory
(`spike-build/`), entirely separate from `build-release/`, so it cannot
corrupt the main app's build cache.

This CI step proves proof point 1 only (buildability on all three OSes). It
proves nothing about points 2–6, which need real hardware and a human.

## Verification

No `QApplication`-based test harness exists in this repo (existing
convention, unchanged by this spike). Where a piece of logic is a pure
function independent of Qt widgets — frame format conversion
(`QVideoFrame`→I420, PCM buffer conversion) — it gets a plain GoogleTest
case with no `QApplication`, consistent with the rest of the test suite.
Everything else is verified manually.

A `spikes/livekit-cpp-spike/README.md` documents the manual checklist,
mirroring roadmap §5.3's six points, for the user to run on real
Linux/Windows/macOS machines:

1. Confirm CI's `Build LiveKit spike` step is green on all three OS matrix
   entries (proves point 1; a red step here means the SDK didn't build on
   that OS and everything below is moot for that OS).
2. Launch the spike; confirm camera + microphone capture starts with no
   crash and no leaked device handle after repeated start/stop.
3. Confirm local preview and remote video render correctly, including
   during window resize.
4. Mid-session, switch camera, microphone, and audio output device from the
   combo boxes; confirm the stream keeps working.
5. Click Leave, then close the window; confirm tracks stop, no dangling
   callback fires afterward, and the process exits cleanly.
6. Run a real call between two machines over the staging LiveKit Cloud
   project — once on a network with direct connectivity, once forcing
   TURN/TLS (e.g. via a restrictive network or LiveKit Cloud's
   connectivity-test tooling).

## Out of scope

- Token backend (#78), domain layer (#79), product call UI (#80) — separate
  issues, explicitly blocked on this spike's result.
- Group calls, recording, screen share, chat — excluded per roadmap §8 for
  all of P1-B, not just this spike.
- Platform-native capture/render fallback code — if Qt Multimedia fails on
  an OS, that is a decision-gate finding to report, not a code path to add.
