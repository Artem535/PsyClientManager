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
