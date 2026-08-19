# Project status — 2026-08-19 (0.2.4)

Milestone 1 (SteamVR overlay + head-look on the legacy framebuffer) is
**working, verified in the headset** as of 0.2.3: no flicker, responsive
head-look, overlay framed correctly at full 1280x960. 0.2.4 starts the
performance work (see "Performance", below).

## Verified on hardware

- **Flicker: fixed.** Root cause was per-frame `SetOverlayRaw` (the
  compositor tears down and recreates the overlay texture on every call).
  Fix = persistent double-buffered D3D11 textures on the HMD's adapter via
  `SetOverlayTexture` (`proxy/steamvr_host.cpp`), plus sequence-gated
  uploads so only new frames are pushed.
- **Head-look: correct** (yaw was mirrored; negated in
  `proxy/camera_hook.cpp`).
- **Clean shutdown: fixed.** Quitting used to leave an unkillable zombie
  XIII.exe wedged in the NVIDIA driver, holding the single-instance lock
  until reboot. `proxy/shutdown_hook.cpp` IAT-hooks `kernel32!ExitProcess`
  and stops the VR host threads before the OS terminates them. Three quit
  cycles verified clean.
- **Overlay framing: fixed.** Windowed XIII asks for a desktop-sized D3D8
  backbuffer but renders its viewport in the top-left corner; the proxy now
  clamps the backbuffer to the window client size (CreateDevice + Reset
  hooks in `proxy/frame_capture.cpp`).

## Performance (0.2.4 — attacks 1 and 2 done, desk-verified)

Attack items 1 and 2 from the 0.2.3 plan are implemented and verified on the
dev machine (Amos01, windowed 800x600–1280x960):

- **Profiling is in.** Every capture phase is QPC-timed and summarized in a
  single-line `[xiii-perf]` heartbeat every 5 s:
  `present=N captured=N skipped=N (cap Nms) | us avg/max: copy=.. lock=..
  decode=.. submit=..`. Capture it with DebugView or the new
  `tools/odscap.ps1` (DebugView-style DBWIN listener; note the older `Log()`
  helpers emit prefix/message/newline as three separate ODS events).
- **First profile (dev machine):** the `CopyRects` GPU->CPU readback dominates
  at **avg ~2.7 ms, max ~9.3 ms** per capture; lock ~0; decode ~440 us (after
  the fast path below); latch memcpy ~180 us. The readback stall is ~6x
  everything else combined, confirming the 0.2.3 hypothesis.
- **The readback is rate-capped**: `[VR] CaptureMinIntervalMs` (default 11 ms
  ~= 90 Hz, 0 = uncapped) gates the whole readback, not just the upload, so
  the render thread pays at most ~90 stalls/s regardless of game fps.
- **Capture is skipped entirely when no VR host is enabled** — with
  `[VR] SteamVR=0` / `OpenXR=0` the game runs the stock path (verified: no
  readbacks, no heartbeat; debug BMP frames still capture on their fixed
  frame numbers, so desk verification is unaffected).
- **Decode rewrote into `capture_core/`** (new unit-tested static lib, plus
  `RateLimiter`/`PhaseStats`): 32-bit frames are now a row `memcpy` + alpha
  pass instead of a per-pixel format branch, and the per-frame
  `malloc`/`free` of the whole frame is a reused buffer. 3 test suites /
  1700+ assertions green (`build/tests/`).

Remaining candidates, in order:

1. **Double-buffered readback (one-frame latency):** `CopyRects` into surface
   A but `LockRect` surface B from the previous capture, so the lock never
   waits on an in-flight copy. Cheap to try; should cut most of the ~2.7 ms
   stall on the render thread.
2. **GPU-only path**: share the D3D8 backbuffer with the D3D11 device (shared
   surface / DXGI interop) and feed `SetOverlayTexture` without touching the
   CPU. Hardest but removes the whole round-trip. Re-profile on the home
   machine first — its `[xiii-perf]` numbers decide whether this is worth it.

## Runtime setup (game side)

- Game: Steam "XIII - Classic", `system\` gets: built `D3DDrv.dll` (proxy),
  stock render device renamed to `D3DDrv_Original.dll`, `openvr_api.dll`.
- `XIII.ini`: `[VR] SteamVR=1`, `CameraLiveHmd=1`; windowed via
  `StartupFullscreen=False` + `UseFullscreen=False`, 1280x960
  (`WindowedViewportX/Y`). Revert to fullscreen by setting both to True.
- Build with `build.bat` (repo root; fix the vcvars32 path per machine).
  Fetch third-party libs per `third_party/*/fetch_*.md` if missing.

## Caveats / notes

- XIII **pauses presentation when its window loses focus** — keep the game
  window focused while in the headset, or frames stop (overlay auto-hides
  after 5 s of starvation and resumes when frames return).
- Single instance: a running/hung XIII.exe blocks new launches silently
  (instant exit code 0).
- Enable exactly ONE of `[VR] SteamVR` / `[VR] OpenXR`. The OpenXR host is
  still unverified on hardware.
- Telemetry: everything logs via OutputDebugString (`[xiii-*]` prefixes);
  capture with DebugView or `odscap.ps1`-style tooling. Debug BMP frames
  land in `%TEMP%\xiii_capture\`.
- On the home PC a `~ HIGHDPIAWARE` compat flag was set for XIII.exe (HKCU
  AppCompatFlags\Layers) — harmless, not required by the mod.
