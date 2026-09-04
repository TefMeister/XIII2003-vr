# 2026-09-04 (`/pd`, home PC, NO LAUNCH) — M2 true stereo is BUILT: route (b) as the backbone plus the vertex-shader-constants path, and the six shaders are identified from disk

**The game was not launched and nothing here has been run.** Every claim below is
`[inferred-static 2026-09-04]`, `[compile-verified 2026-09-04]` or `[verified-numerically 2026-09-04]`
unless tagged otherwise. The deployed build sits BESIDE the installed one, not over it.

---

## 1. What the `[PD]` row asked for, and what was found before writing a line

The board's row: *build route (b) as the backbone plus a vertex-shader-constants path; write the
per-eye constants at `c0`; only 6 shaders exist; do NOT rewrite every projection.* Two questions
were open in the 2026-09-03 note: **what the six programmable shaders draw**, and **whether `c0 x8`
is two matrices**. Both turned out to be answerable from the shipped `D3DDrv_Original.dll` without
the game — the §5 lesson again.

### 1a. The shaders are assembled from TEXT at runtime, and the text is in the DLL

A scan for D3D8 bytecode version tokens (`0xFFFE01xx`) finds **none** in the render DLL; a scan for
shader-assembly mnemonics finds **three `vs.1.0` source strings** at file offsets `0x59050`,
`0x59158`, `0x59278` (plus a detached fog fragment at `0x4E400` that reads `c8.y`/`c8.z`, and six
`xps.1.1` Xbox pixel-shader strings — leftovers from the console port that the PC path cannot even
assemble). `[inferred-static 2026-09-04, n=1 binary]`

What the three vertex shaders do (paraphrased — the text is Ubisoft's and is not reproduced here):

| source | what it does | constants it reads |
|---|---|---|
| `0x59050` | **the cel-shade OUTLINE pass**: normalises the vertex normal, pushes the position along it by `c4.w`, transforms by the 4x4 at `c0..c3`, outputs the constant colour `c4` | `c0..c3` matrix, `c4` = (colour rgb, width w) |
| `0x59158` | **toon lighting lookup, normal-based**: transforms by `c0..c3`, colour `c4`, passes `v2` as texcoord 0, and computes texcoord 1 as `(n·c5, n·c6)` from the normalised normal | `c0..c3`, `c4`, `c5`, `c6` |
| `0x59278` | the same lookup but from the normalised **position** instead of the normal | `c0..c3`, `c4`, `c5`, `c6` |

All three compute `oPos = v0.x*c0 + v0.y*c1 + v0.z*c2 + v0.w*c3` — **position times the 4x4 matrix
whose rows are `c0..c3`**, row-vector order, the same convention as `SetTransform`. So the
2026-09-03 `[hypothesis]` "a cel-shade outline pass" is now `[inferred-static]`: one of the six
shaders IS the outline pass, and the other two sources are the toon shading. Six *handles* from three
*sources* is the driver creating variants (the outline site picks between two global handles, the toon
site between three — see §1b), consistent with the `shaders created=6` the run counted.

### 1b. ⭐ The driver builds `c0..c3` as World × View × Projection from the SAME caches `SetTransform` maintains

The `+0x4205` upload site (outline pass), disassembled:

```
lea eax,[ebx+0x90]  ; push        <- FD3DRenderInterface cache: TT_WorldToCamera  (this+0x90)
lea ecx,[ebx+0x50]  ; push        <- cache: TT_LocalToWorld                          (this+0x50)
lea edx,[esp+0x64]  ; push ; call 0x1161D75A          => tmp = World * View
lea eax,[ebx+0xD0]  ; push        <- cache: TT_CameraToScreen                        (this+0xD0)
lea ecx,[esp+0x60]  ; push
lea edx,[esp+0x14]  ; push ; call 0x1161D75A          => out = tmp * Projection
mov ecx,[eax+8] ; [esp+0x5C..0x64] = 0,0,0 ; [esp+0x68] = ecx   => c4 = (0, 0, 0, width)
push 5 ; lea eax,[esp+0x10] ; push eax ; push 0 ; push esi
call [edx+0x13C]                                       => SetVertexShaderConstant(0, &out, 5)
; then SetVertexShader with one of two global handles ([0x1165FDF4] / [0x1165FE00])
```

The toon site does the identical two multiplies at `+0x4638..+0x4664` from `[ebp+0x50]`,
`[ebp+0x90]`, `[ebp+0xD0]`, then uploads **8** constants at `c0` (`+0x4779`) and binds one of
three global handles — that is the `c0 x8` the run saw, and **it is one matrix plus four parameter
registers, not two matrices**: `c4` colour, `c5`/`c6` lookup vectors, `c7` = `(4,4,1,1)` constants
`[inferred-static 2026-09-04]`. The 2026-09-03 `[hypothesis]` "two stacked matrices" is
**disproved**. The remaining two upload sites are `c10 x1` at device init (`+0x2601`: `0.5`,
`cos`, `sin` of a constant angle — the lookup's fixed parameters) and `c8 x1` (`+0x12947`: fog
parameters; never seen in the run, so the fog appendix path was not exercised).

The multiply helper `0x1161D75A` is an import thunk; the function body next to it is a plain
row-vector 4x4 product (`out[0] = a[0]*b[0] + a[1]*b[4] + a[2]*b[8] + a[3]*b[12]`), so
`call(out, A, B)` is `out = A*B`, and the pushes give `World*View*Projection` — exactly what the
shaders' `v0 * [c0..c3]` needs.

The offsets `+0x50 / +0x90 / +0xD0` are the three caches §7a decoded on 2026-09-01 for the
`SetTransform` early-out, so this is a **second independent use of the same three fields** — the
corroboration rule in §6 of the `/pd` rules, met statically.

**Why this decides the design.** The programmable draws do not read a *different* matrix; they read
the *product* of the three matrices the fixed-function path already receives. Per-eye stereo is
therefore the same operation for both pipelines — replace V and P by their per-eye versions — either
through `SetTransform` (fixed-function draws) or by re-uploading `c0..c3 = W * V_eye * P_eye`
(programmable draws). No matrix inversion, no shader patching, no early-out to manage.

---

## 2. What was built

All in `staging/XIII2003-vr/src/repo` (the M2 working tree). Built with `build.bat` (MSVC, VS2022
Build Tools, Win32, `/MT`): **exit 0, 211,968 bytes, exports 40/40 identical in name to the deployed
0.2.7 `D3DDrv.dll`**, imports `KERNEL32 / USER32 / d3d11 / dxgi`, delay-loads `openvr_api` and
`openxr_loader` as before. `[compile-verified 2026-09-04]`

### 2a. `pose_math/stereo_math.{h,cpp}` — the maths, pure C++

`Mat4Mul` (row-vector), `Mat4ApproxEqual`, `Mat4ProjectPoint`, **`StereoEyeView`**
(`out = view * Translate(-e,0,0)`; left eye `e = -ipd/2`, right `+ipd/2`; parallel cameras, no
convergence), **`StereoEyeProjectionFromTangents`** (off-centre LH projection from OpenXR-style
tangents, with the engine's `Q` and `-zn*Q` copied verbatim so the depth mapping is untouched),
`StereoSplitViewport` (two exact halves, the right one takes the odd pixel).

### 2b. `tests/test_stereo_math.cpp` — 11 cases, **9,209 assertions, 0 failures** `[verified-numerically 2026-09-04]`

Ground truth is built independently of the code under test: the `D3DXMatrixPerspectiveFovLH`
formula written out in double; the geometric statement of parallax (`ndc.x` moves by
`-e * m00 / z`); a test-only general 4x4 inverse. What is checked:

- 1,000 random world points through 40 random views: **the left eye sees every point further right,
  by exactly `ipd * m00 / z` in NDC**; depth unchanged; **zero vertical disparity**; zero IPD gives
  identical eyes.
- **The shipped route equals the algebraically different one:** `W * V_eye * P_eye` (what the hook
  computes) matches `(W*V*P) * P^-1 * T(-e) * P_eye` (peel the projection off the driver's matrix,
  translate, re-project) for 50 random W, V, e and both symmetric and asymmetric per-eye projections.
- Symmetric tangents reproduce the FovLH matrix bit-for-bit within 1e-5; asymmetric frusta put the
  four edges on NDC ±1; near/far survive; orthographic input and degenerate tangents are refused
  with the output untouched.
- The viewport split is adjacent, covering and off-by-one-safe for widths down to 1.

**The tests can fail** (`/pd` rules §6: a negative is only evidence if a positive was possible). Five
deliberate breakages of a scratch copy of `stereo_math.cpp`, run through the unchanged tests:

| mutation | failures |
|---|---|
| eye-shift sign flipped | 2,006 of 9,209 |
| off-centre x term sign flipped | 40 |
| `Mat4Mul` in column-vector order | 250 (and 3 of 11 cases) |
| right half no longer takes the odd pixel | 6 |
| near/far row dropped from the eye projection | 4 |

Evidence: `dev-archive/recon/2026-09-04-m2-stereo-math-verified/`.

### 2c. `proxy/stereo_hook.{h,cpp}` — the device-level hook

Installed from the existing `CreateDevice` hook, **after** the recon hooks (chain: recon → stereo →
real), only when `[VR] Stereo` is non-zero. With `Stereo=0` nothing is patched.

- **Shadows, never ours:** `SetTransform`(37) records W/V/P and whether P is perspective;
  `SetViewport`(40) the engine's viewport; `SetRenderTarget`(31) whether the target is the back
  buffer (by size/format against `GetBackBuffer`); `SetVertexShaderConstant`(79) `c0..c7`;
  `CreateVertexShader`(75)/`SetVertexShader`(76) whether a *programmable* shader is bound, by the
  handle the driver actually got back (declaration-only creations excluded, as in the recon).
- **Draw-twice** in all four `Draw*` hooks (70–73), per draw: **orthographic projection → mono,
  untouched** (HUD, canvas, menus); **non-back-buffer target → mono**; otherwise viewport = left
  half, eye 0 state, draw; right half, eye 1 state, draw; **engine's state restored** (view,
  projection or `c0..c3`, and viewport) before returning.
  - Fixed-function draws: per-eye `D3DTS_VIEW` / `D3DTS_PROJECTION` via the *real* `SetTransform`.
  - Programmable draws: the shadowed `c0..c3` is **compared to the shadowed `W*V*P`**
    (tolerance 1e-3 relative + absolute). Match → upload `W * V_eye * P_eye` per eye, restore the
    engine's `c0..c3` after. **Mismatch → the draw is issued into both halves unmodified** (present
    in both eyes at infinity rather than missing) and counted. The first three matches are logged
    with the max element difference, so the run itself will say whether §1b holds live.
- **Per-eye parameters:** IPD = `[VR] StereoIPD` Unreal units (default 3.4) until the OpenXR host
  publishes located views, then their separation in metres × `[VR] StereoWorldScale` (default 52.5
  u/m — **`[hypothesis]`**, UE folklore, tune with numpad 4/6). Projection = the engine's own
  symmetric 4:3 until per-eye tangents are published, then the asymmetric per-eye frustum
  (`StereoUseHmdFov=1`).
- **Hotkeys (numpad, never F-keys):** 7 toggle, 4/6 IPD −/+ 0.25, 5 swap eyes. `Stereo=2` installs
  the hooks but starts OFF, so a flat run can begin mono and toggle live.
- **Telemetry:** `[xiii-stereo]` + `%TEMP%\xiii_capture\xiii_stereo.log`, one line per second:
  `stereo=ON/off ipd=..u(ini|hmd) fov=engine|hmd vp=WxH | draws/frame: ff-stereo vs-stereo
  vs-mismatch mono-ortho mono-rtt mono-off no-state | rt-switch-away max-mismatch`.
- **Fail-safe:** if any of the ten vtable slots could not be hooked, the ones that were are put back
  and stereo stays unavailable for the session (a half chain would crash).

### 2d. Plumbing

- `vr_host`: `VrHostSet/GetEyeParams` (per-eye tangents + IPD in metres, under the existing critical
  section) and `VrHostSet/GetStereoLayout` (0 mono, 1 SBS, 2 SBS swapped).
- `openxr_host`: after `xrLocateViews` publishes the tangents and view separation; in the
  projection-layer path each view's `subImage.imageRect` becomes **its half** of the swapchain image
  when the layout is SBS. The quad (M1) path is untouched — with SBS on it shows the side-by-side
  picture flat, which is exactly the flat-screen proof.
- `frame_capture`: calls the three stereo entry points; exposes `FrameCaptureHookVtbl`.
- `transform_hook`: new `[VR] TransformReconDumpFromFrame=N` so the recon's verbatim matrix dumps
  start at Present N instead of being spent in the main menu (the 2026-09-03 limitation).

### 2e. Deployed — beside, not over

- `system\D3DDrv-m2-stereo.dll` (211,968 B, md5 `a748d3bb…`) next to the untouched installed
  `D3DDrv.dll` (75,776 B, md5 `f1598329…`, verified after the copy). Also
  `staging/XIII2003-vr/D3DDrv-m2-stereo-2026-09-04.dll`. The 2026-09-02 recon build stays at
  `system\D3DDrv-m2-recon.dll`; this build **contains** the recon and the OpenXR projection path too,
  so it supersedes it for any future run.
- `XIII.ini` `[VR]` gained documented `Stereo=0`, `StereoIPD=3.4`, `StereoWorldScale=52.5`,
  `StereoSwapEyes=0`, `StereoUseHmdFov=1`, `StereoHotkeys=1`, `TransformReconDumpFromFrame=0`
  (backup `XIII.ini.bak-2026-09-04`). All defaults leave behaviour unchanged.

---

## 3. What is NOT established

- **Nothing has rendered.** The draw-twice loop, the state restore, the back-buffer test and the
  constants compare are compile-verified only. The maths is verified; the *plumbing* is not.
- **That `c0..c3 == W*V*P` holds at draw time.** §1b shows the driver composes it that way at upload;
  whether an intervening `SetTransform` ever changes W between upload and draw is what the
  `vs-mismatch` counter and the three logged comparisons will say. If mismatches dominate, the
  fallback design (peel P off the uploaded matrix: `M * P^-1 * T * P_eye`, already cross-checked in
  the test) needs no W at all and is the next thing to build.
- **Which of the 3–8 distinct views per frame is the player camera.** Not addressed: every
  perspective view gets the same eye offset. Skyboxes will not care; a mirror or portal view might.
- **IPD in engine units** (`3.4`, and `52.5` u/m) is folklore. Numpad 4/6 exist for this.
- **Whether anything in XIII renders to an off-screen target of back-buffer size** — such a target
  would pass the size/format test and be stereo-split. The `rt-switch-away` counter will show how
  often targets change at all.
- **The HMD-fov path** needs the headset; the flat run exercises only the engine-projection path.
- **Performance** on the dev PC is non-diagnostic by standing rule; the per-draw cost is two extra
  `SetTransform`s or one 4-register upload, plus two viewport sets.

---

## 4. The one flat run that settles it, and what each outcome means

Rename `D3DDrv.dll` aside (it is 0.2.7; keep it), rename `D3DDrv-m2-stereo.dll` → `D3DDrv.dll`,
set `[VR] Stereo=2`, launch, get into a level, press **numpad 7**. Restore the 0.2.7 DLL afterwards.

| What you see | Meaning |
|---|---|
| Two copies of the world side by side, near objects visibly offset between the halves, **HUD and menus full-width and single** | **M2 render half proven.** Numpad 4/6 to exaggerate the parallax, numpad 5 if depth looks inside-out when cross-viewed. Read `xiii_stereo.log`: `vs-stereo` > 0 and `vs-mismatch` ≈ 0 means the constants path is live too. |
| Two copies, but the **outlines / toon-shaded characters** sit at the same place in both halves while everything else has parallax | The constants path is falling back: `vs-mismatch` will be high. §1b does not hold at draw time → build the `M * P^-1` route. |
| Two copies but **identical** even at a large IPD | The per-eye view is not reaching the device — a plumbing bug (shadow never set, `s_haveV` false: `no-state` high in the log), not a maths one. |
| World in the left half only, right half black or garbage | Second-eye viewport or state not applied — check `mono-rtt`/`no-state`, and whether the engine's viewport was ever seen (`vp=0x0`). |
| HUD doubled or confined to one half | An orthographic pass was classified perspective, or a 2D pass runs under a perspective projection — the `mono-ortho` count against the 12.9/frame the recon measured says which. |
| Skewed or offset geometry in stereo, correct in mono | The derivation is wrong in a way the tests share — report the `xiii_stereo.log` header and the first three `c0..c3 == W*V*P` lines; this is the one outcome that indicts the maths. |
| Anything wrong **in mono** (before numpad 7) | Not stereo's doing: with `s_on` false every hook is a pure passthrough. Suspect the swap/deploy; restore the 0.2.7 DLL. |
| No `[xiii-stereo]` line at all | `Stereo` key not read (wrong ini / wrong DLL active) — the startup log line names the mode. |

Then the headset (`OpenXR=1`, `OpenXrProjection=1`, `Stereo=1`): each eye gets its half, the
per-eye fov switches to `hmd` in the log, and the 2026-09-02c/§12 runtime question rides along.

---

## 5. Files

- `staging/XIII2003-vr/src/repo/pose_math/stereo_math.{h,cpp}`, `tests/test_stereo_math.cpp`,
  `proxy/stereo_hook.{h,cpp}`; edited `vr_host.{h,cpp}`, `openxr_host.cpp`, `frame_capture.{h,cpp}`,
  `transform_hook.cpp`, `build.bat`, three `CMakeLists.txt`.
- `staging/XIII2003-vr/D3DDrv-m2-stereo-2026-09-04.dll`, `BUILD.md`.
- `dev-archive/recon/2026-09-04-m2-stereo-math-verified/README.md` (test + mutation record).
- `engine-research/ENGINE-DOSSIER.md` §7a (shader sources, the `W*V*P` composition, state 2026-09-04).
