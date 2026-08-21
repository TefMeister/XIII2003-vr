# Engine Dossier — XIII (2003) "Classic" (Unreal Engine 2.x)

> Distilled current truth about this game's engine, as learned through the
> `PLAYBOOK.md` phases. Blow-by-blow history lives in the `-dev-archive` and
> `-modding-notes` repos; this is the consolidated reference.

**Status:** Milestone 1 (framebuffer-in-VR + head-look) **working and verified
in a real Quest 3 headset** (via Virtual Desktop → SteamVR) as of build 0.2.3;
0.2.4–0.2.7 are performance and robustness work (capture-path profiling, a
double-buffered-readback experiment, a focus-loss-freeze fix, file-based perf
logging). **VR-readiness verdict:** proven for monoscopic head-look. True
stereo depth and 6DOF/motion-controlled aim are explicitly deferred to a
future Milestone 2 (a native-ABI approach — a pure framebuffer capture can
never feed VR pose back into the simulation).

## 1. Identity
- XIII (2003), the "Classic" original (Ubisoft), PC. Sold on both GOG and
  Steam as "XIII - Classic"; the dev copy is the **Steam** build at
  `Steam\steamapps\common\XIII - Classic\`. (Distinct from the 2020 remake —
  this dossier is the 2003 original only.)
- 32-bit, 2003-era. Owned copy confirmed. Not a modern port.

## 2. Engine lineage
- **Unreal Engine 2.x** (classic UE1/UE2-era architecture), the standard
  pluggable-DLL layout:
  - `XIII.exe` — thin launcher / main loop host.
  - `Core.dll` / `Engine.dll` — native engine + simulation.
  - `D3DDrv.dll` — the swappable **RenderDevice** (Unreal's pluggable renderer
    interface), loaded by name from `Engine.dll`. **This is our injection
    foothold** (§4).
  - `WinDrv.dll` — window / input driver.
  - `*.u` (`xiii.u`, `gameplay.u`, `xidpawn.u`, …) — gameplay logic as
    compiled **UnrealScript bytecode** (decompilable to near-source, so
    gameplay-side RE is far cheaper than native disassembly — a lever not yet
    pulled, relevant for Milestone 2).
- Rotation is the classic Unreal **`FRotator`**: three `int32` in declaration
  order **Pitch, Yaw, Roll**, where a full revolution is **65536 units** (not
  degrees/radians). Getting this convention wrong is the classic "camera
  barely moves / spins wildly" bug, so the conversion is isolated and
  unit-tested (`RadiansToUnrealRotatorUnits`).

## 3. Binary & memory
- 32-bit process — the proxy DLL **must** be built Win32 or it won't load.
- Renderer API: **Direct3D 8** (confirmed: `D3DDrv.dll` imports d3d8; the
  proxy drives D3D8 COM by manual vtable indexing). Monoscopic — the original
  renderer has no stereo path.
- Verified D3D8 vtable method indices (fixed/ABI-stable, used directly, no SDK
  headers needed):
  | Interface / method | vtable index |
  |---|---|
  | `IDirect3D8::CreateDevice` | 15 |
  | `IDirect3DDevice8::Present` | 15 |
  | `IDirect3DDevice8::GetBackBuffer` | 16 |
  | `IDirect3DDevice8::CreateImageSurface` | 27 |
  | `IDirect3DDevice8::CopyRects` | 28 |
  | `IDirect3DSurface8::Release` | 2 |
  | `IDirect3DSurface8::GetDesc` | 8 |
  | `IDirect3DSurface8::LockRect` | 9 |
  | `IDirect3DSurface8::UnlockRect` | 10 |
- The Present hook is reached the standard way — `Direct3DCreate8` →
  `CreateDevice` (vtable 15) → hook `Present` (vtable 15) — **not** by reversing
  D3DDrv's own exports.

## 4. DRM / anti-debug & injection foothold
- **No Steam DRM / anti-cheat / anti-tamper** on this classic build (GOG is
  DRM-free by nature; the Steam "Classic" build behaves the same here). No
  need to design around protection.
- **Injection foothold: proxy `D3DDrv.dll`.** Ship our own `D3DDrv.dll` that
  chain-loads the stock renderer renamed to `D3DDrv_Original.dll` and forwards
  every export straight through, intercepting only what we need. This is
  *idiomatic* for the engine — Unreal loads a RenderDevice by filename, so no
  external injector is required, just dropping a file in `system\`.
  - The forwarding `.def` is generated from `dumpbin /exports` on the stock
    DLL by `tools/generate_forwarding_def.py` (each export forwarded as
    `Name=D3DDrv_Original.Name`).
- Hooks used inside the process are inline (MinHook, or a hand-built 5-byte
  jmp trampoline for the camera hook — see §6). Every hook verifies the
  target's prologue bytes at runtime before patching, so a build/version
  mismatch **fails safe** (skips the hook) instead of crashing.

## 5. Threading & frame structure
- Single-threaded classic-UE main loop in `XIII.exe` (a `CMainLoop`-shaped
  loop: `GIsRunning` / `GIsRequestingExit` globals, a tick-rate limiter driven
  by `Engine->GetMaxTickRate()`). One thread ticks the engine and renders.
- **Focus-gated ticking (important, and a fixed gotcha):** every loop
  iteration calls `user32!GetForegroundWindow()` and, if the foreground window
  belongs to another process (or the game window `IsIconic`), it flags the
  app inactive, runs the engine deactivate path (**which also mutes audio**),
  **skips `Engine->Tick` entirely**, and the tick limiter degenerates into an
  8 ms `appSleep`/`Sleep(8)` poll loop. No ini setting controls this. It's
  pure native code in the main loop — found live in the debugger. This is why
  an unfocused game **freezes presentation within ~1 s**, which is fatal for a
  VR overlay when the user clicks anything else. See §7 for the fix.

## 6. Camera & view-rotation delivery (the crucial section)
- **Milestone 1 does head-look, not stereo.** There is no per-eye projection
  work — the same finished 2D frame goes to both eyes; head *orientation* is
  applied by overriding the game's own view rotation each frame.
- **Override point (function hook):** `APlayerController::eventPlayerCalcView`
  in `Engine.dll`, resolved by its **exported decorated name**
  `?eventPlayerCalcView@APlayerController@@QAEXAAPAVAActor@@AAVFVector@@AAVFRotator@@@Z`
  (no address hardcoding needed — it's an export). That function outputs the
  render camera's `FRotator` each frame; the hook calls the original, then
  **adds** the HMD-derived yaw/pitch to the out `FRotator`.
  - Calling convention: `__thiscall`, emulated as `__fastcall` (`this` in ECX;
    the three reference args — `AActor*&`, `FVector&`, `FRotator&` — on the
    stack).
  - Prologue relocated by the trampoline: `sub esp,1C; mov eax,[esp+20]`
    (`83 EC 1C 8B 44 24 20`, 7 bytes, no relative operands so it copies
    verbatim). Verified at runtime before patching.
- **Axis convention (learned on hardware):** OpenVR/OpenXR yaw winds
  **opposite** to Unreal's, so the HMD yaw is **negated** before adding
  (`CameraRotation->Yaw += RadiansToUnrealRotatorUnits(-eulerYaw)`). This was
  a real "mirrored head-look" bug caught and fixed during a headset session.
  Pitch is signed and small — converted directly (no wrap-normalize). Yaw
  wraps mod 65536, so the normalized `[0, 65536)` value adds correctly.
- **Pose pipeline (unit-tested, pure):** HMD quaternion →
  `QuaternionToEuler` → `RadiansToUnrealRotatorUnits`
  (65536 units/revolution, wrap-safe). Lives in `pose_math/`, TDD.
- Config gates (read from the game's own `XIII.ini`, `[VR]` section):
  `CameraLiveHmd=1` drives the view from live HMD pose; `CameraSyntheticSweep=1`
  is a headset-free smoke test (a self-running ~0.8°/frame yaw sweep) so the
  hook is visually verifiable on the dev monitor. Both default 0 (passthrough);
  LiveHmd wins if both set.

## 7. Frame capture & VR presentation (the D3D8→VR bridge)
- **Two host paths exist; enable exactly one** via `XIII.ini [VR]`:
  `SteamVR=1` (an **OpenVR overlay**, the hardware-verified path) or
  `OpenXR=1` (an OpenXR quad-layer host — **still unverified on hardware**).
  `openvr_api.dll` must be in `system\` for the SteamVR path.
- **Capture:** at `Present`, `CopyRects` the D3D8 backbuffer into a
  system-memory `CreateImageSurface`, `LockRect`, decode, and hand the pixels
  to the VR host. OpenXR/OpenVR have **no native D3D9/D3D8 graphics binding**,
  so the VR host owns its **own D3D11 device** purely for the swapchain/overlay
  texture and copies the captured frame into it.
- **SteamVR overlay, the flicker fix (verified):** feeding the overlay via
  per-frame `SetOverlayRaw` makes the compositor tear down and recreate the
  overlay texture **every call** → flicker. Fix = persistent double-buffered
  D3D11 textures on the **HMD's adapter** via `SetOverlayTexture`, with uploads
  gated on the frame latch's sequence number (only new frames pushed).
- **Overlay framing fix (verified):** windowed XIII asks for a **desktop-sized**
  D3D8 backbuffer but renders its viewport only in the **top-left corner**;
  the proxy clamps the requested backbuffer to the window client size on
  `CreateDevice` **and** `Reset` so the overlay isn't a small image in a big
  black frame.
- **Focus-freeze fix (§5), verified desk-side:** `proxy/focus_hook.cpp`
  IAT-hooks `user32!GetForegroundWindow` **in `XIII.exe` only**, reporting the
  game's own device window while a foreign window holds focus — so the main
  loop keeps ticking. Scoped to the EXE's IAT so **WinDrv/Engine keep their
  honest view of focus** (mouse capture and `WM_KILLFOCUS` unchanged). Gated
  `[VR] KeepRenderingUnfocused` (default on), installed only when a VR host is
  on. Consequences: audio keeps playing while unfocused (mute sits on the same
  branch as the pause — intended for VR); a **minimized** window still pauses.
- **Clean-shutdown fix (verified):** quitting used to leave an unkillable
  `XIII.exe` wedged in the NVIDIA driver, **holding the single-instance lock
  until reboot**. `proxy/shutdown_hook.cpp` IAT-hooks `kernel32!ExitProcess`
  to stop the VR host threads before the OS terminates them.

## 8. Performance notes (this game)
- **The capture-path bottleneck is the `CopyRects` GPU→CPU readback**, not the
  pixel work: dev-machine profile ~2.7 ms avg / ~9.3 ms max per capture, ~6×
  everything else combined (lock ~0, decode ~440 µs, latch memcpy ~180 µs).
- **Rate cap:** `[VR] CaptureMinIntervalMs` (default 11 ms ≈ 90 Hz, 0 =
  uncapped) gates the whole readback so the render thread pays at most ~90
  stalls/s regardless of game fps.
- **Cold path when VR is off:** with no VR host enabled the capture is skipped
  entirely — non-VR players run the stock game untouched.
- **Pipelined (double-buffered) readback** (`[VR] PipelinedReadback`, default
  **off**): `CopyRects` into surface A while `LockRect`ing surface B (whose
  copy was issued one capture interval earlier). Dev-machine A/B showed **no
  win** — on that driver the stall lives *inside* `CopyRects` itself (it syncs
  at blit time), so alternating surfaces can't help. Kept behind a gate because
  a different driver may defer the blit and pay at `LockRect`, which is exactly
  what pipelining fixes — worth one home-machine A/B.
- Remaining lever if the readback stall must be *removed* (not moved):
  **GPU-only shared-surface path** — share the D3D8 backbuffer with the D3D11
  device (shared surface / DXGI interop) and feed `SetOverlayTexture` with no
  CPU round-trip. Hardest option; re-profile on the target machine first.
- **Dev-machine framerate is non-diagnostic** — this is a low-powered dev PC;
  real performance is judged on the home/target machine.

## 9. Config / ini cheat sheet
All keys live in the game's own `XIII.ini` under `[VR]` (everything defaults
off / to stock behavior):
| key | effect |
|---|---|
| `SteamVR` | enable the OpenVR overlay host (hardware-verified path) |
| `OpenXR` | enable the OpenXR quad-layer host (**unverified on HW**) — enable exactly ONE host |
| `CameraLiveHmd` | drive the view rotation from live HMD pose |
| `CameraSyntheticSweep` | headset-free self-sweeping-yaw smoke test |
| `CaptureMinIntervalMs` | readback rate cap (default 11 ≈ 90 Hz; 0 = uncapped) |
| `PipelinedReadback` | double-buffered readback (default 0; see §8) |
| `KeepRenderingUnfocused` | the focus-freeze fix (default on; see §5/§7) |
| `PerfLog` | append `[xiii-perf]` heartbeats to `%TEMP%\xiii_capture\xiii_perf.log` (default on) |

Windowing (stock game keys): `StartupFullscreen=False` + `UseFullscreen=False`
+ `WindowedViewportX/Y` (e.g. 1280×960) for windowed dev testing; both True to
revert to fullscreen.

## 10. Telemetry & harness
- All telemetry via `OutputDebugString` with `[xiii-*]` prefixes — capture with
  DebugView or the repo's `tools/odscap.ps1` (a DBWIN listener). Note the older
  `Log()` helpers emit prefix/message/newline as three separate ODS events.
- Perf heartbeat (`[xiii-perf]`, every 5 s): a single line
  `present=N captured=N skipped=N (cap Nms) | us avg/max: copy=.. lock=..
  decode=.. submit=..`, with `pipe=0/1` naming the readback mode. Since 0.2.7
  it also **appends to `%TEMP%\xiii_capture\xiii_perf.log`** (per-run session
  header, opened/closed per write so it's always flush-safe) — so a headset
  session needs no live listener: play, then send the file.
- Debug BMP frames land in `%TEMP%\xiii_capture\` on fixed frame numbers
  (synchronous, so they show the exact frame and work on the priming tick after
  a device reset) — desk verification without a headset.
- Pure logic is factored into a unit-tested static lib `capture_core/`
  (doctest, TDD): `frame_decode` (32-bit memcpy fast path + alpha pass),
  `perf_stats`, a wrap-safe `RateLimiter`, `readback_ring` (the pipelined-
  readback slot scheduler), and `focus_policy` (the report-which-window
  decision). `pose_math/` holds the quaternion/rotator conversions. 5 doctest
  suites.

## 11. Dead ends & false leads (save future time)
- **Pipelined/double-buffered readback did not help on the dev machine** (§8) —
  the stall is inside `CopyRects`, not at the lock. Not deleted (a different
  driver may benefit), but don't expect it to be the perf win; the GPU-only
  shared-surface path is the real lever.
- **Mirrored head-look** — OpenVR yaw winds opposite to Unreal's `FRotator`;
  the HMD yaw must be **negated** (§6). Symptom: turn left, game pans right.
- **Overlay = small image in a big black frame** — caused by XIII requesting a
  desktop-sized backbuffer while rendering only its top-left viewport; fixed by
  clamping the backbuffer to the client size on CreateDevice + Reset (§7). Not
  an overlay-sizing bug.
- **"Keep the game window focused or frames stop"** — was **not** a VR-host
  bug; it's XIII's own main loop gating `Engine->Tick` on
  `GetForegroundWindow` (§5). Fixed by lying to that one poll (§7).
- **Unkillable zombie `XIII.exe` on quit** holding the single-instance lock —
  fixed by stopping VR host threads before `ExitProcess` (§7). A running/hung
  instance blocks new launches silently (instant exit code 0), so this looked
  like "the game won't launch" until root-caused.

## 12. Open risks toward the North Star
- **True stereo depth** is not attempted in Milestone 1 (same 2D image to both
  eyes). Real per-eye rendering needs Milestone 2's native-ABI direction —
  a pure framebuffer capture can't produce genuinely distinct eye views.
- **6DOF / motion-controlled two-handed weapon aim** requires feeding VR pose
  **into the simulation**, which the framebuffer approach structurally cannot
  do (the sim never sees VR data). That's the whole reason Milestone 2 exists;
  the UnrealScript `*.u` bytecode being decompilable is the lever for it.
- **OpenXR host is unverified on hardware** — only the OpenVR/SteamVR overlay
  path has been confirmed in the headset. Enable exactly one host.
- **Readback cost on the target machine is unconfirmed** — the profiled numbers
  are from the low-powered dev PC; the home-machine `[xiii-perf]` log decides
  whether the GPU-only path is worth building.
