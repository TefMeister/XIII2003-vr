# The HUD needs a THIRD treatment, not zero offset and not the world's — NVIDIA documented it, and the objective marker is in the wrong bucket entirely

**Researched:** 2026-09-11 (`/gr`, estate sweep) · **Project:** `XIII2003-vr` · **Engine:** Unreal Engine 2, Direct3D 8, our patched `D3DDrv.dll`

## Why this matters here, specifically

The first true-stereo headset run (2026-09-10) produced a ⭐⭐ `[PD]` row from the wearer's own words:
*"both eyes line up fine in game but not in the menu … hud elements don't line up, like a objective
marker that is supposed to point exactly what drawer to open, it slides around my view"*.

Two separate things are wrong in that sentence, and the research says they need **different fixes**.
The row currently reads as one problem ("put 2D at a fixed depth"); it is two.

---

## 1. ⭐ The 2D fix is a THIRD treatment, and NVIDIA wrote down the mechanism

NVIDIA's archived GameWorks **"3D Vision Automatic"** best-practices documentation describes exactly the
mechanism our D3D8 patch re-implements, and exactly the fix for 2D `[reported 2026-09-11, first-party
vendor documentation, Rev. 1.0.220830, ©2014–2022]`:

- **How the two views are produced:** the driver duplicates render targets, splits each draw in two, and
  appends a clip-space footer to vertex shaders — **`position.x += Separation * (position.w −
  Convergence)`**, with separation's sign flipped per eye. Convergence is the depth plane at which the
  eyes agree.
- **The rule for 2D**, verbatim: *"2D Rendering is typically the area of the rendering engine that
  requires the most care to get right for a successful stereoscopic title."* And: *"To render an object
  without separation, at the same screen-space position in the left and right eye, the best approach is
  to render these objects at convergence depth."* The value is retrievable at runtime via
  `NvAPI_Stereo_GetConvergenceDepth`; *"if the W coordinate of the output position from the vertex
  shader is at this depth, no separation will occur between each eye."*
- **For world-referenced HUD elements specifically:** draw them *"at an apparent depth value"* matching
  the object they represent — **not** at screen depth.
- Also documented, and useful context for why injectors get this wrong: the driver's own 2D heuristics —
  a NULL Z-buffer is treated as "do not stereoise"; non-square surfaces at or above backbuffer size are
  stereoised, smaller and square ones are not by default.

### ⭐ Read against our `mono-ortho` bucket — this is the actionable part

Our renderer already counts orthographic draws in their own bucket (26 per frame in gameplay), so the
hook exists. But the bucket needs a treatment that is **neither of the two obvious ones**
`[inferred-static 2026-09-11]`:

- **Not "drop the eye offset"** — that pins the HUD at exact screen depth, which is what 3D Vision
  Automatic does by accident and is uncomfortable.
- **Not "give it the world's eye offset"** — that is our current defect.
- **Instead: substitute a perspective projection at one chosen distance `D` for the whole bucket**, so
  the parallax every HUD pixel receives is `Separation * (D − Convergence)` — **constant across the
  element, identical frame to frame, and tunable by one number.** Equivalently, staying in screen space:
  apply a fixed per-eye horizontal pixel shift and nothing else.

The community's name for that one number is **HUD depth**, and **exposing it as an adjustable value
rather than hard-coding it is the convention** — HelixMod/3Dmigoto fixes conventionally bind keys to it
(the UE3 Prototype 2 fix uses `I` for HUD-depth variants and `U` for presets; the UE4 universal fix
ships an `AutoDepthHUD`; several fixes put crosshair depth on F1/F2/F3). We already have numpad 4/6 on
IPD, so adding one more live knob fits what this project already does.

The canonical formula in the shader-hacking community is the same one:
`o0.x += Separation * (o0.w − Convergence)`, with 3Dmigoto exposing `stereoParams.x` as signed
separation and `stereoParams.y` as convergence in `w` units; **the standard recipe for fixing a 2D
element is to substitute a constant for `o0.w`** `[reported]`. ⚠️ bo3b's own "Canonical Stereo Code"
page 404'd on direct fetch and was read via the 3Dmigoto wiki copy and search snippets — weaker
sourcing for the wording, though the formula is corroborated by NVIDIA's own doc above.

---

## 2. ⭐ The objective marker is a DIFFERENT bucket, and this is the half the row currently misses

*"An objective marker that is supposed to point exactly what drawer to open"* is **world-anchored**. It
only looks like HUD because it is submitted through the 2D path.

NVIDIA's guidance splits these explicitly (above): screen-space UI → convergence/fixed depth;
world-referenced indicators → **the depth of the thing they indicate**. So giving the marker the HUD's
flat offset will make it *stop sliding* and still be **wrong** — it will sit at the HUD's distance
instead of at the drawer's.

**The correct treatment is to project it with each eye's own view matrix like any 3D object**, or at
minimum to give it the referenced object's depth `[inferred-static 2026-09-11]`.

The community's implementation of the hard version is 3Dmigoto's **Auto Crosshair**, which *finds* the
right depth per frame: it builds a ray from each eye, walks 255 samples outward from the near plane,
converts depth-buffer samples with `world_z = far*near / (((1−z)*near) + (far*z))`, compares against
`w = (separation*convergence)/(separation − offset)`, and uses the last non-intersecting offset as the
element's X shift. DarkStarSword's toolchain exposes the simpler version as a switch (`shadertool.py` /
`hlsltool.py` / `asmtool.py` can insert a UI-suitable depth adjustment keyed to a constant register
component), and his Far Cry 4 and Akiba's Trip fixes ship auto-HUD / UI-depth toggles.

**⇒ Suggested row change: split the ⭐⭐ row in two.** (a) the 2D bucket gets one tunable fixed depth;
(b) world-anchored markers get the referenced object's depth, or per-eye projection. They are different
fixes and (a) does not imply (b).

---

## 3. How to detect an orthographic projection reliably, and a D3D8-specific trap

Our bucket classification matters, so how it is decided matters. **Check the projection matrix, not the
draw.** An orthographic matrix has `m32 == 0` and `m33 == 1`, so `w` stays 1 and the perspective divide
is a no-op; a perspective matrix has a non-zero `m32` whose sign encodes handedness `[reported
2026-09-11]`. In D3D8 fixed-function terms: `GetTransform(D3DTS_PROJECTION)` with `_34 ≈ 0` and
`_44 ≈ 1`.

⚠️ **And a second, independent 2D signal that matrix inspection cannot see.** **Pre-transformed vertex
formats (`D3DFVF_XYZRHW`) bypass the transform pipeline entirely** — Microsoft's own FVF documentation
describes such vertices as already being in 2D window coordinates. **Any such draw is screen-space by
construction and cannot be stereoised by matrix manipulation at all**; it needs an explicit per-eye X
offset in pixels. `[reported 2026-09-11, first-party docs]`

**This is worth a static check before designing the fix:** nothing public states which of the two UE2's
`D3D8Drv` uses for tiles, and the answer changes the implementation. We can read our own renderer and
find out.

---

## 4. The end state, shared with the sibling UE1 project

The strongest public approach sidesteps per-eye 2D shifting altogether: **render the 2D once into a
texture and draw it as a quad at a chosen distance in the world.**

**UT99 Quest (GhwstVR, 2026)** — the only Unreal-family project found that actually ships stereo —
states: *"The 2D layer is a quad"*, *"they get mapped onto a plane sitting in the world so they have
real depth, and the controller pointer runs that same mapping backwards to work out what you clicked"*,
with menus *"on a panel a couple of metres out, anchored where you opened them"* and the HUD
horizon-locked `[reported]`. **OpenXR standardises the same construct as `XrCompositionLayerQuad`**,
described by Khronos as *"useful for user interface elements or 2D content rendered into the virtual
world"* — and since this project already runs over OpenXR (VDXR), that layer type is **directly
available to us**, which it is not to most injectors.

It is rendered once rather than replayed, automatically correct in both eyes, comfortable by
construction, and it fixes **the menu and the HUD with one mechanism**. ⚠️ GhwstVR also names the cost
they have not paid down: *"The engine currently does a full draw pass per eye where it only needs one
per frame."*

---

## 5. ⚠️ What could not be found, and why that is worth recording

- **Nothing UE2-specific at all on stereo.** No public documentation of UE2's `FRenderInterface` or
  `D3D8Drv` internals — not how Canvas tiles are submitted, not whether they use an ortho matrix or
  pre-transformed vertices, not `SetTransform`/`TT_CameraToScreen`. The UDN "Two" pages cover Canvas
  *UnrealScript*, UnrealEd and terrain, not the C++ render interface. Searches restricted to the Unreal
  docs and the beyondunreal / unrealsp wikis returned nothing.
- **No HelixMod or 3Dmigoto fix exists for any UE2 game** — both tools are D3D9/D3D11, and UE2's D3D8
  path is out of their reach. **So there is no UE2 HUD-depth fix to copy.** The UE3 and UE4 universal
  fixes are the nearest relatives, and the technique transfers even though the code cannot.
- **No XIII-specific stereo or 3D-Vision work of any kind.** No HelixMod post, no vorpX profile thread,
  no Nexus mod.
- **No public UE1 or UE2 render device does stereo**, so our `D3DDrv` patch has no precedent to copy
  per-eye state tracking from — the same finding the sibling `unreal-gold-vr` topic records.
- **Vireio Perception's `D3DProxyDeviceUnreal` HUD handling** exists and would be the closest relative
  (its feature list includes *"HUD and GUI resizing and 3D depth adjustment"*, with the HUD on a
  secondary render target), but the current master is the rewritten v4 whose README says nothing about
  it, and **the two MTBS3D pages documenting the 2.x HUD/GUI depth modes both returned HTTP 403 to
  automated fetch** — so that detail is **unverified** and explicitly not relied on here. ⚠️ Per our own
  standing rule, a 403 is not a negative result; this is a lead someone could open in a browser.

**Dormancy:** NVIDIA's 3D Vision Automatic docs are archived (3D Vision driver support ended 2019) but
remain the best written statement of the technique. bo3b's wiki pages were last edited 2021-01.
Vireio Perception was last substantially reworked 2021.

## 6. What this unlocks

1. **Split the ⭐⭐ row.** The 2D bucket wants one tunable fixed depth (a third treatment, not zero and
   not the world's); the objective marker wants the referenced object's depth or true per-eye
   projection. Fixing the first will not fix the second.
2. **One static read decides the implementation:** does our `D3D8Drv` path submit tiles with an ortho
   matrix or with `D3DFVF_XYZRHW` pre-transformed vertices? Matrix manipulation cannot touch the latter.
3. **`XrCompositionLayerQuad` is available to this project specifically**, because it already runs on
   OpenXR — and it is the same end state as the sibling `unreal-gold-vr` project.

## Sources

All read online; no code copied. Full credit list in `CREDITS.md`.

- **NVIDIA GameWorks** — *3D Vision Automatic: Stereoscopic Issues* and *Background Information*
  (archived), Rev. 1.0.220830, ©2014–2022: the separation/convergence footer, the convergence-depth rule
  for 2D, the apparent-depth rule for world-referenced HUD, and the driver's 2D heuristics.
- **Bo3b Johnson** — 3Dmigoto wiki *Schwing getting started guide* (last edited 2021-01-24) and
  *Auto Crosshair*; *Bo3b's School for Shaderhackers*. ⚠️ the *Canonical Stereo Code* page 404'd on
  direct fetch.
- **DarkStarSword** — `3d-fixes` (`shadertool.py` / `hlsltool.py` / `asmtool.py` UI-depth insertion;
  Far Cry 4 and Akiba's Trip auto-HUD toggles).
- **Helix, Losti, DHR** and others — HelixMod blog, for the HUD-depth and crosshair-depth key
  conventions (Prototype 2 fix, 2013-02; Unreal Engine 4 universal fix, 2019-12, updated 2022-11-10).
- **"admin", xdPixel** — *Decoding a Projection Matrix* (ortho vs perspective via `m32`/`m33`),
  2019-01-30.
- **Microsoft** — Direct3D 9 fixed-function FVF codes documentation (`D3DFVF_XYZRHW` = pre-transformed,
  already in 2D window coordinates).
- **GhwstVR** — *UT99 Quest* (2026), for the quad 2D layer and the per-eye-pass cost note.
- **Khronos OpenXR Working Group** — `XrCompositionLayerQuad`, OpenXR 1.1 registry.
- **cybereality / Denis Reischl** and contributors — Vireio Perception (`D3DProxyDeviceUnreal`
  referenced in MTBS3D development threads) — recorded as an **unverified** lead, 403 on fetch.
