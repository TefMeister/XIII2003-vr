# 2026-09-03 (`/lm`, dev PC, ONE flat launch) — the M2 route is decided by measurement: **route (b) alone is not enough**

The run the board had been waiting for. XIII launched flat on the dev PC with the
`TransformRecon` build as the active render device, ~5.5 minutes of varied play (menu, indoor,
outdoor, cutscene), then quit. **26,595 frames, 343 one-second intervals, 2,076,870 `SetTransform`
calls and 3,254,942 draws.** No `HANDLE TABLE OVERFLOWED` marker, so the counts are trustworthy in
the one direction that mattered.

Log rescued to `dev-archive/recon/2026-09-03-m2-draw-recon/xiii_transform.log`.

---

## 1. ⭐ The headline: the programmable path draws real geometry, constantly

`[measured 2026-09-03, n=26,595 frames, one session]`

| | value |
|---|---|
| intervals with `programmable-VS > 0` | **338 of 343** |
| frame-weighted programmable share | **8.72 %** (283,774 of 3,254,942 draws) |
| peak interval | **51.4 %** of that second's draws |
| fixed-function draws / frame | min 1.0, mean 123.5, max 487.8 |
| programmable-VS draws / frame | min 0.0, mean 11.8, max **113.7** |
| distinct vertex shaders created | **6**, all session |

**The static risk from 2026-09-02b materialised.** That note found ten `CreateVertexShader` call
sites and predicted that a pure route (b) — draw every batch twice at D3D8 with per-eye
`D3DTS_VIEW`/`D3DTS_PROJECTION` — would leave any programmable draw mono, while flagging that
whether those draws carry *world geometry* was "a count, not a static question".

It is now a count. **The recon's own verdict line fired on 339 of 343 intervals:**

```
=> some draws bypass SetTransform via vertex-shader constants:
   route (b) needs a constants path too, or those draws stay mono
```

against only **4** intervals of `=> every draw this interval was fixed-function`. The 8.72 % is
not a rounding error hiding in menus: the near-zero intervals are menus and loads, and the busiest
gameplay seconds are the ones where the programmable share is *highest*.

**⇒ The decision: neither route alone. M2 needs the D3D8 draw-twice path AND a vertex-shader
constants path.** Route (b) remains the backbone — it covers 91 % of draws with no early-out to
defeat — but it must be paired with a per-eye constants write, or roughly one draw in eleven, and
half the frame during effect-heavy moments, stays flat inside a stereo image.

## 2. ⭐ `c0 x5` predicted statically, confirmed live — and it is not alone

2026-09-02b disassembled the `SetVertexShaderConstant` site at `+0x4205` and found it uploading
**five constants starting at register `c0`**, inferring "a transform matrix plus one spare" from the
shape alone. The live `VSconst regs` line, identical in **342 of 343 intervals**:

```
VSconst regs: c10 x1  c0 x8  c0 x5
```

- **`c0 x5` is confirmed live** `[verified-live 2026-09-03, n=342 intervals]` — the static
  inference was right, and this is where a per-eye matrix has to go.
- **`c0 x8` was not predicted** — eight float4s at `c0`. Two 4-row matrices back to back is the
  obvious reading (world-view-projection plus something else), but that is
  **`[hypothesis]`** until the consuming shader is read.
- **`c10 x1`** — one float4 at `c10`, the shape of a colour or scalar parameter, not a transform.
  It is the only register touched in the one interval that saw no programmable draws.

**Only 6 shaders exist in the whole session.** The constants path is a bounded surface, not an
open-ended one — six shaders and three register patterns is small enough to handle exhaustively
rather than heuristically.

## 3. The early-out is a much smaller obstacle than route (a) feared

Cumulative `equals-cache` skips over the whole session, against total calls of each type:

| transform | calls | early-out would fire | rate |
|---|---|---|---|
| world | 1,325,094 | 3,536 | **0.267 %** |
| view | 361,272 | 3,536 | **0.979 %** |
| projection | 390,502 | **0** | **0 %** |

`[measured 2026-09-03]` The engine almost never re-sets an identical matrix, and **never** re-sets
an identical projection.

**⚠️ What this does and does not mean.** It bounds how often the early-out is even *reached* in
vanilla play — it does **not** retire the design requirement. Route (a)'s hazard was never
frequency: it is that if the cache is left holding *our* modified matrix, the engine's next
identical-looking set is skipped and the second eye silently inherits the first eye's view. That
requirement — the cache must hold what the engine thinks it set, not what we wrote — stands exactly
as dossier §7a states it. What changed is that the *cost* of getting it right is small, so route (a)
is no longer the expensive option it looked like.

## 4. The projection decode works, and most projections are 2D

`pose_math::DecodeD3DPerspective` produced a clean read on the one perspective projection that
landed in the dump budget:

```
fovY=68.996 deg  aspect=1.33333  near=5.0000  far=65541.00  offcentre=(0.00000, 0.00000)
```

**fovY ≈ 69°, aspect exactly 4:3, symmetric frustum** `[verified-live 2026-09-03, n=1]`. For VR both
the aspect and the fov have to be replaced per eye, and the frustum becomes asymmetric — so this is
the number a stereo projection has to override, and 4:3 confirms it is the game's own render aspect
rather than anything display-derived.

Per frame the engine issues a mean of **12.9 orthographic** projections against only **2.8
perspective** ones. The recon labels the ortho ones itself — *"a 2D / canvas pass, leave alone for
stereo"* — and that ratio is the useful part: **most projection traffic is HUD/canvas and must not
be touched**, so a stereo path that blindly rewrites every projection would corrupt the interface
far more often than it would fix the world.

Also measured: **distinct view matrices per frame — mean 3.3, mode 4.0 (130 intervals), max 8.**
The engine already renders several distinct views per frame in vanilla. That is encouraging for a
draw-twice design (multi-view is not alien to this renderer) but it also means **"the view matrix"
is not a single thing** — a stereo path has to know which of the 3–8 views is the player camera.
Which one is `[hypothesis]`; nothing here identifies it.

## 5. ⚠️ A limitation of this run, worth fixing before the next one

**`TransformReconDump=12` dumps the first 12 matrices of each type, and all 12 were spent in the
menu** — every `world`/`view`/`proj` dump in the log carries `frame=574`, before gameplay started.
So the matrix dumps show identity matrices and a menu camera, and **not one gameplay matrix was
captured.** The per-interval statistics are unaffected (they ran for the whole session and are what
decided the route), but the qualitative half of the recon produced nothing usable.

The counters answered the question this run existed to ask, so this cost us nothing *this time*.
Next run wants either a much larger dump budget, a frame threshold, or a key-triggered dump, so the
budget is spent on gameplay rather than on the main menu.

## 6. What is NOT established

- **What the programmable draws actually are.** The recon counts them and reads their constant
  registers; it does not identify the geometry. XIII is a cel-shaded game and an outline/toon pass
  is an obvious candidate for a dedicated vertex shader, but that is **`[hypothesis]`** — nothing
  here distinguishes an outline pass from skinned characters or from a particle system. Six shaders
  is few enough to settle by dumping their bytecode.
- **Whether `c0 x8` is two matrices.** Inferred from size, exactly the way `c0 x5` was inferred
  before this run corroborated it — so the inference has a decent track record here, but it is still
  an inference.
- **Which of the 3–8 distinct views per frame is the player camera.** Not addressed.
- **Nothing was built or changed.** This run was pure measurement; the recon build has been swapped
  back out and the 0.2.9 build restored.
- The 51.4 % peak comes from one ~10-second stretch (t=245–253 s). **What was on screen then was
  not recorded**, so "effect-heavy moment" is a guess from the numbers, not an observation.
