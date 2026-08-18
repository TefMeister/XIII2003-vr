# Project status — 2026-08-19 (0.2.3)

Milestone 1 (SteamVR overlay + head-look on the legacy framebuffer) is
**working, verified in the headset**: no flicker, responsive head-look,
overlay framed correctly at full 1280x960.

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

## NEXT: performance

Framerate in-game is not good. Most likely cost: the capture path does a
full-backbuffer GPU->CPU readback on the game's render thread every Present
(`CopyRects` + `LockRect` + per-pixel decode in `Hook_Present`), then the
SteamVR thread re-uploads the same pixels CPU->GPU. Candidate attacks, in
rough order:

1. Profile first: time `Hook_Present` (readback vs decode) and check the
   `stats:` heartbeat in DebugView for latch vs upload rates.
2. Rate-cap the capture (e.g. skip readback unless >= 11 ms since the last
   one) — the overlay upload is already capped at ~90 Hz, but the readback
   currently runs at game fps.
3. GPU-only path: share the D3D8 backbuffer with the D3D11 device (e.g. via
   a shared surface / `IDirect3DDevice8` -> DXGI interop, or GDI-free
   cross-device copy) and feed `SetOverlayTexture` without touching the CPU.
   Hardest but removes the whole round-trip.

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
