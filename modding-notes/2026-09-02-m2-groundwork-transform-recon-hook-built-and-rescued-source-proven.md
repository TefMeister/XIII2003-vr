# 2026-09-02 — Milestone 2 groundwork: the `SetTransform` recon hook is built, and the rescued source is proven to be the real thing

**Home PC, static only. The game was not launched, and nothing here has been run.** Every claim
carries its own tag; untagged reads as `[hypothesis]`.

This picks up the two threads left open on 2026-09-01: the M2 stereo hook found in the dossier (§7a,
decoded on the dev PC) and the proxy source rescued into `staging/XIII2003-vr/src/` on the home PC.
Three things moved, none of them needing the game up.

## 1. The rescued source builds here, and it is exactly the last pre-0.2.7 deployment

`[verified 2026-09-02 by build + byte comparison]`

- `staging/XIII2003-vr/src/repo` configures and builds with CMake against VS2022 Build Tools
  (`-A Win32`, `RelWithDebInfo`, the configuration `BUILD.md` records for 0.2.7): `pose_math`,
  `D3DDrv.dll` and the doctest binary all build (the CMake route reports no C++ warnings; the
  `build.bat` route prints only the pre-existing `_snprintf` C4996 deprecation notes). The unit tests pass
  (**10/10**, 24 assertions — 6 pre-existing, 4 new, see §3).
- A straight `build.bat` build of the untouched 0.2.3-era tree produced a `D3DDrv.dll` of
  **157,184 bytes**, and the game folder's `D3DDrv.dll.pre-027-bak` (the DLL that ran in the
  headset before 0.2.7 was deployed on 2026-08-23) is also 157,184 bytes. `cmp -l` reports
  **6 differing bytes**, at file offsets `0x110` and `0x21464` — the link timestamp in the PE file
  header and its copy in the debug directory. Nothing else. Their string tables are identical
  (659 lines each). **So the rescued tree is not "0.2.3-era, roughly": it is the source of the
  build that was actually deployed here before 0.2.7.**
- The fresh build's export table is **identical to the shipped 0.2.7**: 40 exports, all
  forwarders to `D3DDrv_Original.dll`, including `?GetXIIIStats@UD3DRenderDevice@@UAEXPAMH@Z`.
  Import tables identical too (`d3d11`, `dxgi`, UCRT; `openvr_api` and `openxr_loader` both
  delay-loaded). `proxy.def` is therefore complete.
- **Build prerequisite the tree does not carry:** `third_party/openvr/lib/openvr_api.lib` and
  `third_party/openxr/lib/openxr_loader.lib` are gitignored redistributables. The fetch
  instructions in `third_party/*/fetch_*.md` work as written (the two `curl` lines, x86/Win32
  builds). Without them the link step fails on the first `openvr_api` symbol.

## 2. What the lost 0.2.4→0.2.9 delta actually contained, from the binary

`[inferred-static 2026-09-02 — strings of the shipped 0.2.7 DLL vs the fresh 0.2.3 build]`

Present in the shipped `D3DDrv.dll` (0.2.7, 75,776 bytes, in `staging/XIII2003-vr/` and in the
game folder) and **absent** from anything the rescued source can produce:

| Feature (BUILD.md) | Evidence in the 0.2.7 binary |
|---|---|
| 0.2.6 focus-loss fix | `GetForegroundWindow IAT-hooked in XIII.exe (keep rendering unfocused)`, `[xiii-focus]`, `KeepRenderingUnfocused`, `disabled via [VR] KeepRenderingUnfocused=0` |
| 0.2.7 perf log | `PerfLog`, `xiii_perf.log`, `[xiii-perf] present=… captured=… (cap %ums pipe=%d)` |
| 0.2.5 pipelined readback | `PipelinedReadback`, `pipe=%d`, `capture: vr consumer=%d, min interval=%u ms, pipelined=%d, perf log=%d` |
| rate cap | `CaptureMinIntervalMs` |

Two things follow.

- **The delta is bigger than `BUILD.md` says.** BUILD.md records the loss as 0.2.4→0.2.7. But the
  2026-08-27/28 notes describe **0.2.8 and 0.2.9** — the automation harness (external command
  file, `UCheatManager` lookup by exported vtable, BEGIN/END logging, telemetry) that was **verified
  in game** on the dev PC. No `Automation` / `xiii_automation` string exists in the 0.2.7 binary, and
  no source for it exists anywhere on this machine. **The dev PC's game folder may still hold the
  0.2.9 `D3DDrv.dll`** — if so, it is the only surviving embodiment of the harness and belongs in
  `staging/XIII2003-vr/` next to the 0.2.7 one. `[hypothesis — needs a look at the dev PC]`
- **0.2.7 is a strict superset of what this machine can rebuild.** That is why nothing was
  deployed today (see §5).

## 3. The Milestone 2 recon hook — written, compile-verified, not run

`[compile-verified 2026-09-02; the hook has never executed]`

Two new files in the proxy, `transform_hook.cpp/.h`, plus a projection decoder in `pose_math`:

- **A recon-only inline hook on `FD3DRenderInterface::SetTransform`** (`D3DDrv_Original.dll +
  0x129E0`, dossier §7a). Same technique as `camera_hook.cpp`: a 5-byte `jmp` over the prologue
  (`push ebp; mov ebp,esp; push -1` = `55 8B EC 6A FF`, no relative operands) into a hand-built
  trampoline. **It changes nothing the engine does** — every call is forwarded to the original with
  the same arguments. It only counts and samples:
  - per second: `SetTransform` calls per frame by type (world / view / projection), how many of the
    view matrices per frame are **distinct** (main view vs skybox vs anything else), how many
    projections are perspective vs orthographic (HUD/canvas), and how often the incoming matrix
    **equals the engine's cache** (= how often the original early-outs — the trap §7a warns about,
    now measured instead of feared);
  - the first 12 matrices of each type verbatim, with each projection decoded into
    `fovY / aspect / near / far` (or flagged infinite-far / orthographic / neither).
  - Output: `%TEMP%\xiii_capture\xiii_transform.log` + DebugView (`[xiii-xform]`).
- **Gated:** `[VR] TransformRecon=1` in `XIII.ini` (default **0** — with it off the hook is not even
  installed, so the shipped behaviour is byte-for-byte unaffected). `TransformReconDump=N` sets the
  per-type sample count (default 12).
- **Fails safe on two independent anchors** before patching: the 6-byte prologue signature
  (`55 8B EC 6A FF 68`, the `push imm32` operand deliberately not compared because it relocates)
  **and** the `ret 8` at `+0x72`. Either mismatch → log a line, patch nothing.
- **`DecodeD3DPerspective()`** (`pose_math`) inverts the D3D8 `PerspectiveFovLH` shape
  (`m[11]==1, m[15]==0`) into `fovY / aspect / near / far`, flagging the infinite-far (`Q==1`) form
  and rejecting orthographic matrices. **Four new tests build the ground truth independently**
  (straight from the `D3DXMatrixPerspectiveFovLH` formula, not from the decoder) and require the
  inputs back: 90°/4:3/1..65536, 37.5°/16:9/8..4096, the infinite-far form, and an ortho reject.
  `zFar` is tolerated at 1% because `Q = zf/(zf-zn)` is ill-conditioned at large `zf`.
- Housekeeping in the same change: `proxy/CMakeLists.txt` was missing `shutdown_hook.cpp`
  (`build.bat` had it; the CMake list was stale — the 0.2.3 CMake build would have shipped without
  the clean-shutdown hook). Fixed. `frame_capture` exposes `FrameCaptureGetPresentCount()` so the
  recon can express counts per frame without a second `Present` hook.

## 4. The §7a decode re-read on this machine's `D3DDrv_Original.dll`

`[inferred-static 2026-09-02, second static reading — every offset and constant agrees with the
dev PC's 2026-09-01 decode; same Steam depot, so possibly the same bytes: this corroborates the
READING, it is not a second USE. Still never run.]`

Verified line by line with `static-disasm.py at +0x129E0`: prologue bytes as above; type 0 →
cache `this+0x50`, `push 0x100` (`D3DTS_WORLD`); type 1 → `this+0x90`, `push 2` (`D3DTS_VIEW`);
type 2 → `this+0xD0`, `push 3` (`D3DTS_PROJECTION`); device via `[[this+4]+0x66C]`, call
`[vtbl+0x94]` (index 37 = `IDirect3DDevice8::SetTransform`); `ret 8` at `+0x72` and `+0xBE`.

Two details the dossier did not have, both relevant to the eventual M2 injection:

- **Order of operations differs by type.** Type 0 calls D3D **then** copies the matrix into the
  cache; types 1 and 2 copy into the cache **first**, then call D3D. Either way the cache ends up
  holding exactly what D3D was given — so a hook that passes a *modified* matrix to the original
  will leave the modified matrix in the cache, and the engine's next *unmodified* set will compare
  unequal and go through. Useful, and the recon's equals-cache counters will show how often the
  engine re-sets an identical matrix in the first place.
- **The compare helper (`+0x9420`) is 16 `fcomp`s, not a `memcmp`**, so `-0.0 == 0.0`; and its
  `test ah,0x44 / jp` idiom treats an *unordered* (NaN) compare as equal. Irrelevant for real
  matrices; noted so the mirror in `EqualsCache()` (which uses `!=`, NaN-unequal) is not "fixed" to
  match by accident.

## 5. Not deployed — and the exact reason

The installed `System\D3DDrv.dll` is **0.2.7**, and 0.2.7 embodies source that exists nowhere else
(§2). The M2 build is 0.2.3 + the recon hook: it **lacks** the focus-loss fix, the perf log and the
rate cap. Overwriting 0.2.7 with it would trade a headset-verified build for one that reintroduces
the "keep the game focused or frames stop" freeze. So the built DLL was **not** copied into the game
folder. It is committed to the private staging repo as
**`staging/XIII2003-vr/D3DDrv-m2-transform-recon-2026-09-02.dll`** (72,704 bytes).

## 6. The one live check, whenever the user chooses to spend a launch on it

Nothing in this note is confirmed until this runs. It costs one launch and about a minute of play.

1. In `C:\Steam\steamapps\common\XIII - Classic\System\`: rename `D3DDrv.dll` → `D3DDrv.dll.027-keep`
   (**do not lose it — it is the only 0.2.7**), copy the staging M2 DLL in as `D3DDrv.dll`.
2. In `XIII.ini` under `[VR]`, add `TransformRecon=1`. Keep `SteamVR=1` / `CameraLiveHmd=1` as they
   are, or set `SteamVR=0` for a headset-free desk test — the recon does not need VR on.
3. Launch normally, load into any level, look around for ~30 s, quit. **Keep the game window
   focused** — this build does not have the 0.2.6 focus fix.
4. Restore: rename `D3DDrv.dll.027-keep` back to `D3DDrv.dll`. (`TransformRecon=1` in the ini is
   inert with 0.2.7 — it does not know the key.)
5. Send `%TEMP%\xiii_capture\xiii_transform.log`.

What each outcome means:

| In the log | Meaning |
|---|---|
| `hooked at …` then heartbeats with **view ≈ 1 distinct/frame, proj persp ≈ 1/frame** (plus ortho ≈ HUD passes), decoded `fovY` matching the game's FOV setting, `aspect` matching the window | **§7a is right and M2 has its injection point.** The counts also settle the design question of how many cameras a frame carries. |
| `SetTransform prologue mismatch` or `ret-anchor mismatch` | The bytes on this machine differ from the static decode — **the derivation is wrong, not a knob.** Attach the DLL's size/hash. |
| Heartbeats, but the main projection reads `neither perspective nor orthographic` | The **matrix-layout assumption** (D3D row-major, `m[11]==1`) is wrong for what UE2 hands over — derivation problem, revisit before any injection. |
| `view distinct` ≫ 1 per frame | Several cameras per frame (skybox, mirrors, portals). Not a bug: a design input for stereo — each will need its own eye offset. |
| **No `xiii_transform.log` at all** | The gate did not fire: check `[VR] TransformRecon=1` is under the right section of the right `XIII.ini`, and that `D3DDrv.dll` really is the M2 build (72,704 bytes). |
| Crash at start with `TransformRecon=1` that goes away with `=0` | The hook itself. `=0` is byte-identical to no hook, so the game is never stuck. |

## What is NOT established

- The hook has **never executed**. Everything about it is compile-time and static-read evidence.
- `ETransformType` 0/1/2 = `TT_LocalToWorld / TT_WorldToCamera / TT_CameraToScreen` is UE2
  convention matched to the D3D constants they forward to — `[inferred-static]`, the names are not in
  the binary.
- The projection decoder assumes UE2 hands D3D a matrix already in D3D's layout. `SetTransform`
  passes the same pointer straight to `IDirect3DDevice8::SetTransform` with no transpose, which is
  strong static support — but only the live decode (a plausible `fovY`) proves it.
- Whether the dev PC still holds a 0.2.9 DLL is a guess until someone looks.
