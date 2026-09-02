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
- **UnrealScript: the source is public, so this is a read, not a decompile**
  (`/gr`, folded 2026-09-02). Earlier notes described the `*.u` bytecode as
  "decompilable to near-source — a lever not yet pulled". Better than that: the
  **Xbox branch of a 2005 leak is mirrored on GitHub** with the full package tree
  and comments intact (`artism90/xiii-unrealscript` archived, `Ch0wW/xiii_unrealscript`,
  `VideogameSources/XIII`; `XIIIArmes` = weapons, `XIIIPersos` = characters,
  `XIDPawn` = AI/scripted pawns, **not** the player). The PC packages are
  `StripSource`d — bytecode intact, text gone — so UE Explorer still earns its keep
  for **PC-vs-Xbox diffs**, but the reading is done against the leak.
  `[reported 2026-09-02 — study only, nothing copied, rights holder Ubisoft]`
  See §12 for what this settles about the M2 aim design.
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

## 7a. Milestone 2: the true-stereo hook (static, 2026-09-01)

`[inferred-static - decoded from D3DDrv_Original.dll and Engine.dll on the dev PC 2026-09-01, re-read
line by line on the home PC's copy 2026-09-02 with every offset and constant agreeing; two static
readings of the same Steam depot, never run]`

UE2 does not deliver view/projection through a UE1-style `SetSceneNode`. It goes through the
render interface, and XIII's implementation is one small function that forwards straight to
Direct3D 8:

**`FD3DRenderInterface::SetTransform` = `D3DDrv.dll + 0x129E0`**
(VA `0x116129E0` at the preferred base `0x11600000`; the module has a `.reloc`, so always work in
RVA). `__thiscall`, `ret 8`: `void SetTransform(ETransformType type, const FMatrix *m)`.

Found from the UE2 `guard()` string `"FD3DRenderInterface::SetTransform"` (`0x1164EF6C`), whose
only reference is that function's guard block.

```
if (matrix equals the cached copy) return;          // 0x11609420 = 64-byte compare
dev = *(void **)(*(char **)(this + 4) + 0x66C);     // the D3D8 device wrapper
dev->vtbl[0x94](dev, <state>, m);                   // IDirect3DDevice8::SetTransform (index 37)
memcpy(cache, m, 64);                               // FMatrix = 4x4 floats
```

| `type` | UE2 meaning | cached at | forwarded as |
|---|---|---|---|
| `0` | `TT_LocalToWorld` | `this + 0x50` | `0x100` = `D3DTS_WORLD` |
| `1` | `TT_WorldToCamera` | `this + 0x90` | `2` = `D3DTS_VIEW` |
| `2` | `TT_CameraToScreen` | `this + 0xD0` | `3` = `D3DTS_PROJECTION` |

Confidence comes from two independent facts agreeing: the constants `256 / 2 / 3` are exactly
D3D8's `D3DTS_WORLD` / `D3DTS_VIEW` / `D3DTS_PROJECTION`, and vtable byte offset `0x94` is
index 37, which is `IDirect3DDevice8::SetTransform`.

Two details from the 2026-09-02 re-read, both relevant to the injection design
`[inferred-static 2026-09-02]`: the **order of operations differs by type** - type 0 calls D3D *then*
copies into the cache, types 1 and 2 copy into the cache *first* and call D3D after; either way the
cache holds exactly what D3D received, so a hook that forwards a *modified* matrix leaves the
modified matrix in the cache and the engine's next *unmodified* set compares unequal and goes
through. And the compare helper (`+0x9420`) is **16 `fcomp`s, not a `memcmp`** (`-0.0 == 0.0`; a
NaN compare counts as equal via its `test ah,0x44 / jp` idiom). The second `ret 8` sits at `+0xBE`.

**Why this is the M2 hook.** Both halves of stereo are here: per-eye view = translate
`TT_WorldToCamera` along the camera right axis by +/- IPD/2; per-eye projection = an asymmetric
frustum in `TT_CameraToScreen`. One function covers every pass, and no shader work is involved.
The mod already replaces `D3DDrv.dll`, so it can hook here or one layer down at
`IDirect3DDevice8::SetTransform` - prefer here, because `type` says what the matrix *is* and the
engine's own cache is visible.

### The trap: the unchanged-matrix early-out

`SetTransform` **returns doing nothing when the incoming matrix equals its cached copy.** Modify
the view for eye 1, and when the engine sets the same matrix again for eye 2 it compares equal to
the cache and is skipped - eye 2 silently inherits eye 1's view and stereo collapses with no error
anywhere. Any implementation must keep the cache holding what the engine *thinks* it set while
sending the modified matrix to D3D, or invalidate the cache between eyes. **Settle this before
writing the hook.**

### Route (b): draw every batch twice at the D3D8 level — no early-out to defeat, but a gap of its own

`/gr` (2026-09-02) points at **`unreal-gold-vr`'s M2 design**, which is
`[compile-verified 2026-09-02; verified-numerically 2026-09-02, 70,550 checks]` in that project:
draw **every world batch twice**, each eye through its own half-width viewport with its own per-eye
constants, one shared depth buffer, 2D/HUD left full-width mono, off by default and
console-switchable. Applied here: hook `SetTransform` only to **observe** the engine's view and
projection, and hook `Draw(Indexed)Primitive` to draw twice with per-eye `D3DTS_VIEW` /
`D3DTS_PROJECTION` and viewports. **`FD3DRenderInterface`'s cache never receives a modified matrix,
so the compare behaves exactly as stock and there is nothing to design around.** Prior art at the
same layer: NVIDIA's stereo driver rated UT2004 (same UE2 generation) "excellent" by intercepting
the fixed-function projection per draw, below the engine `[reported 2026-09-02]`.

> #### ⚠️ Route (b) is NOT automatically complete — measured statically 2026-09-02
>
> It covers only draws that take their transform from the **fixed-function** pipeline. A vtable-call
> scan of `D3DDrv_Original.dll` `[inferred-static 2026-09-02, n=1 binary, whole-.text scan]` finds
> the programmable path is **present and used**:
>
> | `IDirect3DDevice8` method | vtable idx | call sites in `D3DDrv_Original.dll` |
> |---|---|---|
> | `CreateVertexShader` | 75 | **10** |
> | `SetVertexShader` | 76 | **17** |
> | `SetVertexShaderConstant` | 79 | **4** |
> | `DrawIndexedPrimitive` | 71 | 12 |
> | `DrawPrimitive` | 70 | 6 |
> | `DrawIndexedPrimitiveUP` | 73 | 3 |
> | `SetTransform` | 37 | 15 |
>
> and **the site at `+0x4205` uploads five constants starting at register `c0`**
> (`push 5` / `lea eax,[esp+0x10]; push eax` / `push 0`), which is the shape of a transform matrix
> plus one spare. A draw issued under a programmable vertex shader reads its view and projection
> from those constants, **not** from `SetTransform` — so route (b) alone would leave exactly those
> draws mono while everything around them went stereo. That is the silent-miss failure `/gr` warned
> about, and it is now known to be a real risk in this binary rather than a hypothetical.
>
> **What is NOT established:** whether those programmable draws carry *world geometry* (they may be
> confined to skinning, terrain or effects), and what fraction of a real frame they are. That is a
> count, not a static question — which is what the draw recon below exists to measure.
>
> **`SetVertexShader` is not a reliable classifier on its own:** D3D8 overloads one `DWORD` for both
> FVF codes and shader handles, and "an FVF has bit 0 clear" is a runtime convention, not something
> to bet a design on. The recon therefore records what `CreateVertexShader` actually **returned** and
> tests membership — and ignores declaration-only creations (`pFunction == nullptr`), which still run
> fixed-function and would otherwise overcount the gap.

### Supporting addresses (`Engine.dll`, preferred base `0x10000000`)

| Symbol | VA |
|---|---|
| `FCameraSceneNode::FCameraSceneNode(UViewport*, AActor*, FVector, FRotator, float Fov)` | `0x103CC190` |
| `FLevelSceneNode::Render(FRenderInterface*)` | `0x103CBFF0` |
| `FLevelSceneNode::GetViewFrustum()` | `0x103CB560` |
| `FLevelSceneNode::GetWorldFrustumPoints(FVector*)` | `0x103CBCD0` |
| `FSceneNode::Deproject(const FPlane&)` | `0x103C9A40` |
| vtables | `FRenderInterface` `0x1046F298`, `FSceneNode` `0x1046F42C`, `FCameraSceneNode` `0x1046F474` |
| `UD3DRenderDevice::Lock` -> `FRenderInterface*` | `D3DDrv` `0x1160DBF0` |

The `FCameraSceneNode` constructor takes location, rotation and FOV in one call, so building the
camera per eye is a viable second route. `SetTransform` is cheaper and does not require
reproducing engine behaviour; the constructor route is the fallback if the cache problem above
turns out to be intractable.

### State (2026-09-02, home PC, static only)

- **Source unblocked.** The proxy source is in `staging/XIII2003-vr/src/repo` (rescued 2026-09-01) and
  **builds on the home PC** (CMake, VS2022, Win32 RelWithDebInfo; unit tests 10/10). A fresh build of
  the untouched tree matches the game folder's `D3DDrv.dll.pre-027-bak` except for the PE timestamps,
  so the rescued tree is exactly the last pre-0.2.7 deployment `[verified-numerically 2026-09-02, n=1 binary]` — an exact byte comparison of the locally rebuilt DLL against the deployed one, which is the whole population, not a sample. The
  0.2.4-0.2.9 delta (focus fix, perf log, pipelined readback, automation harness) survives only as the
  installed 0.2.7 binary - see the 2026-09-02 note, section 2.
  - **⚠️ THE PRISTINE RESCUE IS COMMIT `e67e15e`, NOT THE CURRENT TREE — corrected 2026-09-02.**
    The rule here used to read "DO NOT EDIT ANYTHING INSIDE `staging/XIII2003-vr/src/repo/`",
    because the byte-identity above is the entire evidence that this tree is the deployed build.
    **That rule has already been overtaken by events and saying otherwise would mislead:** the
    2026-09-02 02:54 `/pd` checkpoint (`0b235cb`) added `proxy/transform_hook.*`, `pose_math/` and
    `tests/` into that tree and edited `frame_capture.cpp`, `dllmain.cpp`, `build.bat` and
    `proxy/CMakeLists.txt`; a later pass the same day extended the draw recon. **The tree is now the
    M2 working tree, and a fresh build of it no longer reproduces `D3DDrv.dll.pre-027-bak`.**
    - **Nothing was lost, and the claim is still checkable:** `e67e15e` (2026-09-01, "XIII: rescue
      the proxy source into git") holds the untouched tree — verified 2026-09-02 by listing it:
      no `transform_hook.*`, no `pose_math/`, original `frame_capture.cpp`. **The byte-identity claim
      is anchored to `e67e15e`**, and re-verifying it means building *that* commit, not `HEAD`.
    - **The part of the old rule that still stands:** do not "tidy" the frozen artefacts in there.
      `docs/STATUS.md` is a 2026-08-19 (0.2.3) snapshot and reads as an untagged claim-bearing
      document to tooling; `/gs` check 3 flagged it on 2026-09-02 and the **scanner** was changed to
      skip vendored/rescued trees (`*/src/*`) rather than the file being "fixed". If another tool
      flags something in there, fix the tool.
- **Recon hook written and compile-verified, never run** `[compile-verified 2026-09-02]`:
  `proxy/transform_hook.cpp` - a passthrough inline hook on `SetTransform`, gated behind
  `[VR] TransformRecon=1` (default off = not installed), fail-safe on two byte anchors, logging calls
  per frame by type, distinct views per frame, perspective vs orthographic projections, how often the
  early-out would fire, and the first matrices verbatim with the projection decoded to
  `fovY / aspect / near / far` (`pose_math::DecodeD3DPerspective`, 4 tests against independently built
  ground truth). Built DLL: `staging/XIII2003-vr/D3DDrv-m2-transform-recon-2026-09-02.dll`. **Not
  deployed** - it is 0.2.3-based and would replace the only 0.2.7.
- **Draw / vertex-shader recon added 2026-09-02 (second pass), so ONE launch decides the design.**
  `[compile-verified 2026-09-02]` The transform recon alone counts `SetTransform` traffic but has no
  **denominator** — it cannot say whether the fixed-function pipeline accounts for every draw, which
  is precisely what route (b) depends on. `frame_capture.cpp` now also hooks (behind the same
  `TransformRecon` flag, on the device vtable it already owns) `CreateVertexShader` (75),
  `SetVertexShader` (76), `SetVertexShaderConstant` (79), `DrawPrimitive` (70),
  `DrawIndexedPrimitive` (71) and `DrawIndexedPrimitiveUP` (73), and reports per frame:
  **fixed-function draws vs programmable-VS draws**, the percentage, `SetVertexShader` /
  `SetVertexShaderConstant` rates, how many programmable shaders were created, and the distinct
  `(register, count)` pairs seen on `SetVertexShaderConstant`. Hot-path discipline is preserved: the
  draw hooks bump one counter, the membership scan happens in `SetVertexShader`.
  - **Reading the result:** `programmable-VS=0` across varied scenes ⇒ route (b) covers everything
    and is the better design (no early-out to defeat). `programmable-VS > 0` ⇒ route (b) needs a
    vertex-shader-constants path too; the logged `VSconst regs` line then says which register the
    matrix lands on, and `c0 x5` would corroborate the `+0x4205` static finding.
  - A `HANDLE TABLE OVERFLOWED` marker in the log means the 64-entry shader table filled and the
    programmable count **understates** — say so rather than trusting the number.
- **Built, linked and staged for the run** `[compile-verified 2026-09-02]`: full MSVC build of the
  tree (`build.bat`, VS2022 Build Tools, Win32), 173,056 bytes, **exports verified byte-for-byte
  identical in name and count (40/40) to the deployed 0.2.7 `D3DDrv.dll`**, so it is ABI-compatible
  with what the engine loads. (The size differs from the deployed 0.2.7's 75,776 because of build
  configuration — `/MT` static CRT — not missing functionality; the export surface is the thing that
  decides whether the render device loads.) The vendored OpenVR/OpenXR binaries are gitignored by
  design and were fetched per `third_party/*/fetch_*.md` to complete the link.
  - **Deployed side-by-side, NOT over the live build:** the game folder now has
    `system\D3DDrv-m2-recon.dll` beside the untouched `D3DDrv.dll`. **This is deliberate** — the
    recon build comes from the 0.2.3-era rescued tree, so swapping it in permanently would regress
    the focus-loss IAT hook, perf log and automation harness that survive only in the installed
    0.2.7 binary. Also at `staging/XIII2003-vr/D3DDrv-m2-recon-2026-09-02b.dll`.
  - `XIII.ini` `[VR]` gained documented `TransformRecon=0` / `TransformReconDump=12` keys (backup:
    `XIII.ini.bak-2026-09-02`), so the run is a rename plus one character.
- **Next: one launch** — rename `D3DDrv.dll` aside, rename `D3DDrv-m2-recon.dll` to `D3DDrv.dll`, set
  `[VR] TransformRecon=1`, play a minute of varied scenery, then read
  `%TEMP%\xiii_capture\xiii_transform.log` and **restore the 0.2.7 DLL afterwards**. The step list and
  what each outcome means are in
  `modding-notes/2026-09-02-m2-groundwork-transform-recon-hook-built-and-rescued-source-proven.md`
  section 6, and `modding-notes/2026-09-02b-draw-recon-decides-the-m2-route.md`.

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
| `Automation` | external console-command injection + telemetry (default off; see §9a) |
| `AutomationPollMs` | how often the command drop-file is read (default 200) |
| `AutomationTelemetryMs` | telemetry line interval (default 1000; 0 = off) |
| `AutomationEngineExec` | allow the `UGameEngine::Exec` tier (default **off** — see §9a) |

## 9a. Console commands & the automation harness (verified live 2026-08-27)

XIII ships a real console (**F2**) whose commands can also be driven from
**outside the process**, with no simulated input, via the 0.2.9 automation
harness: append a line to `xiii_automation_cmds.txt` next to `XIII.exe`, and
the proxy executes it and truncates the file. Focus-independent by design.

**⛔ `UGameEngine::Exec` is unsafe to call from a hook in this build — do not
re-arm it.** `[verified-live 2026-08-28, n=2 — two GPFs, two different call
sites]` It is gated behind `AutomationEngineExec` in `[VR]`, **default 0, and it
must stay 0.**

0.2.8 drained the queue from the camera hook (`eventPlayerCalcView`) and the
game died with a **General protection fault** the first time a command arrived:

```
History: UGameEngine::Exec <- UGameEngine::Draw <- UWindowsViewport::Repaint
         <- UWindowsClient::Tick <- ClientTick <- UGameEngine::Tick <- ...
```

`eventPlayerCalcView` is reached from inside `UGameEngine::Draw`, so the
original diagnosis was "console command re-entrant mid-render", and the fix was
to move dispatch to the game-logic phase — `APlayerController::Tick` (prologue
`55 8B EC 6A FF`, 5 bytes, no relative operands). Commands then ran cleanly and
**that diagnosis was recorded here as fact.**

**It was wrong — `[disproved 2026-08-28]`.** The engine tier was re-armed on the
reasoning "the render path was the cause, and dispatch has moved", and the very
first `Button bUp` faulted again from the *safe* site:

```
History: UGameEngine::Exec <- TickAllActors <- ULevel::Tick <- (NetMode=0)
         <- TickLevel <- UGameEngine::Tick <- UpdateWorld <- MainLoop
```

Same fault, game-logic phase, no render path anywhere in the stack. The call
site was never the cause: **`UGameEngine::Exec` itself faults when called from
this harness in this build.** Moving the dispatch site appeared to fix it only
because the two safe tiers (PlayerController, CheatManager) handled every
command actually being sent, so the engine tier was never exercised again until
it was deliberately re-armed.

Consequences worth keeping:

- The generic advice **"never dispatch engine commands from a render-path
  hook"** is still sound practice and still worth following — but it is *not*
  what this crash proves, and it must not be cited as though XIII demonstrated
  it. XIII demonstrates something narrower and more useful: this engine's
  `UGameEngine::Exec` is not callable from a hook at all.
- Losing the tier costs nothing. It only ever offered `Button`, `Axis` and
  `set`; every command the harness relies on lives in the two safe tiers, and
  input is driven through the keyboard route in §9b instead.
- **Method note:** "the fix worked" is not evidence of *why* it worked when the
  failing path stopped being exercised at the same time. A fix that removes the
  symptom and removes the test coverage together has proved nothing.

**Where the commands live — three different objects.** Calling
`UObject::ScriptConsoleExec` on the PlayerController finds only the
controller's own exec functions. The cheats are on **`UCheatManager`**, which
the console reaches by hopping from the controller. Measured live:

| resolves on | commands |
| --- | --- |
| **APlayerController** | `Fov <n>`, `BehindView <0/1>` |
| **UCheatManager** | `God`, `Fly`, `Ghost`, `Walk`, `MaxAmmo`, `HealMe <n>`, `PlayersOnly` |
| **not present in this build** | `Teleport`, `AllAmmo`, `Loaded`, `Invisible`, `SetSpeed`, `ChangeSize`, `Slomo` |

Finding the CheatManager: its **vtable is exported**
(`??_7UCheatManager@@6B@`), so the harness scans the controller's fields for a
pointer whose target's vtable is exactly that — identity, not a hardcoded
offset. Found at `controller+0x598` in this build; re-validated on use because
the CheatManager is destroyed on level change.

**Symbol → module map** (looking in the wrong module returns null *silently*):

| symbol | module |
| --- | --- |
| `?GEngine@@3PAVUEngine@@A` | Engine.dll |
| `?Exec@UGameEngine@@UAEHPBDAAVFOutputDevice@@@Z` | Engine.dll |
| `?Tick@APlayerController@@UAEHMW4ELevelTick@@@Z` | Engine.dll |
| `??_7UCheatManager@@6B@` | Engine.dll |
| `?ScriptConsoleExec@UObject@@UAEHPBDAAVFOutputDevice@@PAV1@@Z` | **Core.dll** |
| `?GLog@@3PAVFOutputDevice@@A` | **Core.dll** |

The mangled names decode to `char const*` (`PBD`), so this build is **ANSI, not
Unicode** — commands pass as plain `char*`. `GLog` supplies the
`FOutputDevice&` both dispatch calls require, so no vtable has to be
fabricated and command output lands in the game's own log.

**End-to-end verified 2026-08-27:** `HealMe 100` sent from a text file raised
the player's health 50 → 100, witnessed in-game. `God`/`Fly`/`Ghost`/`Walk`/
`PlayersOnly` all resolve to the CheatManager; ~20 commands ran across two
sessions with no fault and a clean process exit.

Windowing (stock game keys): `StartupFullscreen=False` + `UseFullscreen=False`
+ `WindowedViewportX/Y` (e.g. 1280×960) for windowed dev testing; both True to
revert to fullscreen.

## 9b. Driving the player — full locomotion without the mouse (verified live 2026-08-28)

The console tiers give *state* (cheats, FOV) but no movement: `Teleport` is
absent from this build, so there is no console way to reposition the player.
Movement comes from synthetic **keyboard** input, and the result is precise
enough to navigate by dead reckoning.

**The mouse is a hard dead end.** `[verified-live 2026-08-28, n=1]` XIII takes the
mouse through **DirectInput in exclusive mode**, so `SendInput` never reaches it: 600 px of injected motion
produced **0.0°** of yaw, while keyboard input in the same session worked
perfectly. Do not spend time on mouse injection, `mouse_event`, or cursor
warping — the device is not reading the Windows input queue at all. (Psychonauts
hit the identical wall; treat exclusive-mode DirectInput as the default
assumption for this era rather than a surprise.)

**Yaw exists but ships unbound.** UE2 input is alias-based, and XIII defines
turn aliases it never binds to a key:

```
Aliases[4] =(Command="Axis aBaseX Speed=-150.0", Alias="TurnLeft")
Aliases[5] =(Command="Axis aBaseX  Speed=+150.0", Alias="TurnRight")
Aliases[26]=(Command="button b90Left",            Alias="FastTurnL")
Aliases[27]=(Command="button b90Right",           Alias="FastTurnR")
```

Binding them to spare keys routes yaw through the keyboard path that already
works — no code change, no injection, no mouse.

**⚠️ Edit `DefUser.ini`, NOT `User.ini`.** `[verified-live 2026-08-28]` XIII does not merely rewrite
`User.ini` on exit — it **deletes** it. The file exists only while the game
runs and is recreated at launch from `DefUser.ini` (verified: identical section
layout, identical line numbers; and no config exists anywhere outside the game
directory — no Documents, no AppData, no Steam userdata). So `User.ini` cannot
be edited with the game closed (it does not exist) and edits made while it runs
are discarded. `DefUser.ini` is the only durable place a binding survives.
Bindings go inside `[Engine.Input]`.

Added for automation (keys verified free in this build):

| key | alias | effect |
| --- | --- | --- |
| `U` / `J` | TurnLeft / TurnRight | smooth yaw |
| `H` / `K` | FastTurnL / FastTurnR | snap turn |

**Measured control model** `[measured 2026-08-28, via tools/xnav.ps1 + harness telemetry]`
(all figures live, not assumed):

| axis | keys | measurement |
| --- | --- | --- |
| move fwd/back/strafe | `W` `A` `S` `D` | **~157 uu/s**, equal in all four directions |
| smooth yaw | `U` / `J` | **∓166 °/s** |
| snap yaw | `H` / `K` | **∓65.4° per tap** (4 trials, spread 0.4°) — *not* the 90° the alias name implies |
| pitch | `Backspace` / `=` | **~148 °/s**, clamps at **±85°** |
| jump | `Space` | — |

Accuracy `[measured 2026-08-28, n=1 route each]`: a four-leg square (W→D→S→A, equal
durations) closed to **3.5 uu on
~190 uu legs (1.8%)**. Closed-loop turning — turn, re-measure, correct — lands
a target heading within **0.8°** in two or three iterations, which is what makes
"turn to heading X, then walk N units" a reliable primitive rather than a nudge.

**🔺 Do NOT wrap the telemetry yaw.** `[measured 2026-08-28]` XIII's `rot=` yaw is a
**raw accumulating integer**, not an angle masked to 0–65535 — it runs straight through the 65536
boundary (observed 179348 → 88646 monotonically). Applying the usual
shortest-arc wrap to it destroys information: a real −199° turn reads as
**+161°**, which looks exactly like the turn key spontaneously reversing
direction. That artifact cost a full round of "the turn keys are unstable"
investigation before five identical presses (−99.0, −98.1, −99.3, −102.3,
−99.4) showed the keys were perfectly stable and the *analysis* was the bug.
Subtract raw values; wrap only for display.

**Focus caveat.** Console commands are focus-independent, but synthetic keys are
not — they follow the foreground window. `KeepRenderingUnfocused=1` keeps the
engine *ticking* when alt-tabbed, which is not the same thing: an unfocused
game keeps rendering but receives no keys. Any driving session must hold
foreground.

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
- **Re-arming `AutomationEngineExec`** — GPFs the game (§9a). The tier is
  disarmed by default and must stay that way; the render-path explanation that
  once justified re-arming it is disproved.
- **Mouse injection for view control** — `SendInput` is ignored outright;
  DirectInput exclusive mode (§9b). 600 px → 0.0° of yaw. Bind keys instead.
- **Editing `User.ini` to add bindings** — the file is deleted on exit and
  regenerated from `DefUser.ini` at launch (§9b). Edits there always vanish; the
  template is the only durable target.
- **Trusting `FastTurnL`/`FastTurnR` to be 90°** — the aliases are named
  `b90Left`/`b90Right` but measure a repeatable **65.4°** (§9b). Measure the
  primitive; don't take the identifier's word for it.
- **Wrapping the telemetry yaw to shortest arc** — it is already unwrapped, and
  wrapping makes stable turn keys look like they reverse direction (§9b).
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
  do (the sim never sees VR data). That's the whole reason Milestone 2 exists.
  **The aim design is now concrete, and it is a READ rather than a decompile**
  (`/gr`, folded 2026-09-02): XIII's **UnrealScript source is public** — the Xbox
  branch of a 2005 leak, mirrored on GitHub (`Ch0wW/xiii_unrealscript`,
  `VideogameSources/XIII`), full package tree including `XIIIArmes` (weapons) and
  `XIIIPersos` (characters). PC packages are `StripSource`d, so decompile those
  only to *diff* against the leaked text.
  `[reported 2026-09-02 — study only, nothing copied, rights holder Ubisoft]`
  - **Fire chain** (`Engine/Classes/Weapon.uc`, both `ProjectileFire` and
    `TraceFire`): `GetAxes(Instigator.GetViewRotation(), X,Y,Z)`;
    `Start = GetFireStart(X,Y,Z)` (native: eye + `FireOffset` in view axes);
    `AdjustedAim = Instigator.AdjustAim(AmmoType, Start, 0)`. **Direction is the
    controller's `Rotation`; origin is the eye.**
  - **So:** head = HMD by *replacing* `CameraRotation` in the existing
    `eventPlayerCalcView` hook; hand = controller by writing motion-controller
    yaw/pitch into the PlayerController `Rotation` from the `APlayerController::Tick`
    hook the harness already owns, so every `GetViewRotation()` the weapon makes
    returns the hand. Origin stays at the eye unless native `GetFireStart` is hooked.
    Disable `ViewAdjustAim` smoothing and bob/shake for comfort. Note XIII keeps a
    firing aim distinct from the displayed one (`AdjustedAimForFiring`,
    `ViewAdjustAim`, `OldAdjustAim`), and `AdjustAim`'s body is commented out on
    this branch.
- **⚠️ The OpenXR question splits in two (`/gr`, folded 2026-09-02) — and the M2 half
  does not exist yet.** `[verified-static 2026-09-02, from Khronos's published `openxr.h`]`
  - **M1: the quad-layer host is unverified on hardware** — only the OpenVR/SteamVR
    overlay path has been confirmed in the headset. Enable exactly one host.
  - **M2: there is no projection-layer path at all.** Both existing hosts are M1
    designs presenting one flat image, and `XrCompositionLayerQuad` is a flat
    rectangle with a single pose that **cannot carry stereo**. Verifying the quad
    host proves M1 over OpenXR and nothing about M2.
  - The good news: `XrCompositionLayerProjectionView` carries **its own `pose` and
    its own `fov`**, and `XrCompositionLayerProjection` holds an **array** of them
    submitted together in one layer, in one space — so per-eye poses are expressible
    by construction, with none of the "last submit wins" collision behind OpenVR
    #1253 (re-checked 2026-09-02: still open, seven years, no Valve response).
    Session and swapchain handling are shared with the quad host, so the new path is
    not large — but it is a different code path, and better known before the
    `SetTransform` work starts producing two eyes.
  - **Untested:** whether a given runtime *honours* independent per-view poses during
    reprojection. Risk moved from "impossible" to "untested per runtime" `[reported]`.
    **`far-cry-2-vr` is blocked on the identical question** for its AER submission — one
    headset test (two views submitted with deliberately different poses) answers it for
    both projects.
- **Readback cost on the target machine is unconfirmed** — the profiled numbers
  are from the low-powered dev PC; the home-machine `[xiii-perf]` log decides
  whether the GPU-only path is worth building.
