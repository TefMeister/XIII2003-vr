# Project status — 2026-08-20 (0.2.6)

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

### 0.2.5 — double-buffered readback: built, gated, no win on the dev machine

Attack candidate 1 (pipelined readback) is implemented and A/B-tested on the
dev machine (Amos01, windowed 1280x960):

- **Mechanism:** two cached system-memory surfaces scheduled by a new
  unit-tested `capture_core/readback_ring` (4th doctest suite). Each capture
  tick `CopyRects` into slot A and `LockRect`/decode slot B, whose copy was
  issued a full capture interval (~11 ms) earlier — so the lock never waits
  on the copy just issued. Costs one capture interval of overlay latency.
  Debug BMP frames always capture synchronously (exact-frame BMPs, and they
  work on the priming tick).
- **Gate:** `[VR] PipelinedReadback` (default **0** = off, the
  hardware-verified 0.2.4 behavior). The heartbeat reports the mode as
  `pipe=0/1`.
- **Dev-machine A/B result: no difference.** Pipelined `copy` avg ~3.3–4.5 ms
  vs. synchronous ~3.3–4.3 ms; `lock` ~0 in BOTH modes. On this driver the
  stall lives INSIDE `CopyRects` (it syncs at blit time), so alternating
  destination surfaces cannot help. That is why the gate defaults off.
- **Worth one A/B on the home machine anyway** (flip the ini key, compare
  `copy` avg/max between `pipe=0` and `pipe=1` heartbeats): a different
  driver may defer the blit and pay the wait at `LockRect` instead, which is
  exactly the case pipelining fixes.

### 0.2.6 — the focus-loss freeze is fixed (desk-verified)

The worst headset-session caveat — *"XIII pauses presentation when its window
loses focus"* — is gone. Root cause (found live in the debugger): XIII.exe's
main loop polls `GetForegroundWindow()` every iteration and, when the
foreground window belongs to another process, skips `Engine->Tick` entirely
and idles in an 8 ms `appSleep` poll loop (caught mid-`Sleep(8)` at zero
CPU). No ini setting controls it.

- **Fix:** `proxy/focus_hook.cpp` IAT-hooks `user32!GetForegroundWindow` in
  **XIII.exe only**, reporting the game's device window while a foreign
  window is foreground (decision logic unit-tested in
  `capture_core/focus_policy`, 5th doctest suite). WinDrv/Engine keep their
  honest view of focus, so input capture and `WM_KILLFOCUS` handling are
  unchanged.
- **Gate:** `[VR] KeepRenderingUnfocused` (default **on**), and the hook is
  only installed at all when a VR host is enabled — with VR off the stock
  path is untouched.
- **Desk-verified:** heartbeats continue at full rate through a 25 s focus
  steal (previously they stopped dead within a second).
- Side effects to know about: the engine's mute-on-deactivate is skipped
  together with the pause, so audio keeps playing unfocused (intended for
  VR). A *minimized* game window still pauses (the loop `IsIconic()`s the
  reported window). The overlay's 5 s starvation auto-hide should no longer
  trigger from focus changes.

Remaining perf candidates, in order:

1. **GPU-only path**: share the D3D8 backbuffer with the D3D11 device (shared
   surface / DXGI interop) and feed `SetOverlayTexture` without touching the
   CPU. Hardest but removes the whole round-trip — and after the 0.2.5
   finding that `CopyRects` itself carries the stall, it is the only lever
   left that can remove that cost rather than move it. Re-profile on the home
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

- ~~XIII pauses presentation when its window loses focus~~ — **fixed in
  0.2.6** (see above) whenever a VR host is enabled. A minimized window
  still pauses; opt out with `[VR] KeepRenderingUnfocused=0`.
- Single instance: a running/hung XIII.exe blocks new launches silently
  (instant exit code 0).
- Enable exactly ONE of `[VR] SteamVR` / `[VR] OpenXR`. The OpenXR host is
  still unverified on hardware.
- Telemetry: everything logs via OutputDebugString (`[xiii-*]` prefixes);
  capture with DebugView or `odscap.ps1`-style tooling. Debug BMP frames
  land in `%TEMP%\xiii_capture\`.
- On the home PC a `~ HIGHDPIAWARE` compat flag was set for XIII.exe (HKCU
  AppCompatFlags\Layers) — harmless, not required by the mod.
