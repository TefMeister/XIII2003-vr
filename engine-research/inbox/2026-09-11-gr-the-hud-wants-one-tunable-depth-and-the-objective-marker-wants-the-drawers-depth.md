# The HUD row is two rows: one tunable depth for the 2D bucket, and the drawer's depth for the marker

**From:** `/gr` (estate sweep, 2026-09-11) · **For:** the modding lane, to fold into
`ENGINE-DOSSIER.md` — the stereo-bucket section, and the ⭐⭐ HUD/menu row

**Full write-up:** [`external-research/topics/2026-09-11-the-hud-needs-a-third-treatment-and-the-objective-marker-is-in-the-wrong-bucket.md`](../../external-research/topics/2026-09-11-the-hud-needs-a-third-treatment-and-the-objective-marker-is-in-the-wrong-bucket.md)

The wearer's sentence contains **two different defects**, and the row currently treats them as one.

## 1. ⭐ The 2D bucket needs a THIRD treatment, and the vendor documented it

NVIDIA's archived GameWorks **3D Vision Automatic** documentation describes the same mechanism our
patch implements — the driver duplicates render targets, splits each draw in two, and appends
**`position.x += Separation * (position.w − Convergence)`** to vertex shaders, separation's sign
flipped per eye — and states the 2D rule directly: *“2D Rendering is typically the area of the
rendering engine that requires the most care to get right”*, and **“to render an object without
separation, at the same screen-space position in the left and right eye, the best approach is to render
these objects at convergence depth”** `[reported 2026-09-11, first-party vendor docs, Rev. 1.0.220830]`.

**Read against our `mono-ortho` bucket** (26 draws/frame in gameplay — the hook already exists), the
treatment is neither obvious option `[inferred-static 2026-09-11]`:

- **Not “drop the eye offset”** — that pins the HUD at exact screen depth, which is what 3D Vision
  Automatic does by accident and is uncomfortable.
- **Not “give it the world's eye offset”** — our current defect.
- **Instead:** substitute a perspective projection at one chosen distance `D` for the whole bucket, so
  every HUD pixel gets parallax `Separation * (D − Convergence)` — **constant across the element,
  identical frame to frame, tunable by one number.** Or, staying in screen space: a fixed per-eye
  horizontal pixel shift and nothing else.

The community name is **HUD depth**, and **the convention is to expose it on a key rather than hard-code
it** (HelixMod/3Dmigoto fixes bind it routinely — the UE3 Prototype 2 fix uses `I`/`U`, the UE4 universal
fix ships `AutoDepthHUD`, several put crosshair depth on F1/F2/F3). We already have numpad 4/6 on IPD,
so one more live knob fits. ⚠️ Per this account's standing rule, keep it on the numpad, not an F-key.

## 2. ⭐ The objective marker is a DIFFERENT bucket — the half the row misses

*“An objective marker that is supposed to point exactly what drawer to open”* is **world-anchored**. It
only looks like HUD because it is submitted through the 2D path.

NVIDIA's guidance splits them explicitly: screen-space UI → convergence/fixed depth; world-referenced
indicators → **the depth of the thing they indicate** (*“an apparent depth value”*). **So giving the
marker the HUD's flat depth will stop it sliding and still leave it wrong** — at the HUD's distance
instead of the drawer's. Correct treatment is per-eye projection with the real view matrix, or at
minimum the referenced object's depth.

The published hard version is 3Dmigoto's **Auto Crosshair**, which finds the depth per frame: a ray from
each eye, 255 samples outward from the near plane, depth samples converted as
`world_z = far*near / (((1−z)*near) + (far*z))`, compared against
`w = (separation*convergence)/(separation − offset)`, taking the last non-intersecting offset as the X
shift. DarkStarSword's toolchain exposes the simpler version as a switch.

**⇒ Suggested row change: split the ⭐⭐ row in two.** (a) 2D bucket → one tunable fixed depth;
(b) world-anchored markers → the referenced object's depth or true per-eye projection. **(a) does not
imply (b).**

## 3. ⚠️ A D3D8 trap that decides the implementation — and one static read settles it

**Detect ortho by the projection matrix, not the draw:** an orthographic matrix has `m32 == 0` and
`m33 == 1` so `w` stays 1; a perspective matrix has non-zero `m32` whose sign encodes handedness
`[reported 2026-09-11]`. In D3D8 fixed-function terms, `GetTransform(D3DTS_PROJECTION)` with
`_34 ≈ 0`, `_44 ≈ 1`.

**But there is a second 2D signal matrix inspection cannot see.** **`D3DFVF_XYZRHW` pre-transformed
vertices bypass the transform pipeline entirely** — Microsoft's own FVF documentation describes them as
already in 2D window coordinates — so **any such draw is screen-space by construction and cannot be
stereoised by a matrix edit at all**; it needs an explicit per-eye pixel offset
`[reported 2026-09-11, first-party docs]`.

**Nothing public states which of the two UE2's `D3D8Drv` uses for tiles.** We can read our own renderer.
**This is the cheapest next step and it changes the design.**

## 4. ⭐ The end state is available to THIS project in particular

Render the 2D once into a texture and submit it as a quad at a chosen distance in the world. **UT99
Quest** (GhwstVR, 2026) does exactly this — *“the 2D layer is a quad”*, mapped onto a plane with real
depth, menus on a panel a couple of metres out, HUD horizon-locked, with the controller pointer running
the mapping backwards for clicks. OpenXR standardises it as **`XrCompositionLayerQuad`**, described by
Khronos as *“useful for user interface elements or 2D content rendered into the virtual world”*.

**Because this project already runs over OpenXR (VDXR), that layer type is directly available to us** —
which it is not to most injectors. It fixes the menu and the HUD with one mechanism, and it is the same
end state the sibling `unreal-gold-vr` project is heading for. ⚠️ GhwstVR names the unpaid cost: a full
draw pass per eye where one per frame would do.

## 5. ⚠️ Gaps, recorded as gaps rather than dead ends

- **Nothing UE2-specific on stereo exists publicly.** No documentation of `FRenderInterface` or
  `D3D8Drv` internals — not how Canvas tiles are submitted, not ortho-vs-pre-transformed, not
  `SetTransform`/`TT_CameraToScreen`. The UDN “Two” pages cover Canvas *UnrealScript*, UnrealEd and
  terrain, not the C++ render interface.
- **No HelixMod or 3Dmigoto fix exists for any UE2 game** — both are D3D9/D3D11 and UE2's D3D8 path is
  out of reach. **There is no UE2 HUD-depth fix to copy**; the UE3/UE4 universal fixes are the nearest
  relatives, and the *technique* transfers even though the code cannot.
- **No XIII-specific stereo or 3D-Vision work of any kind** — no HelixMod post, no vorpX profile
  thread, no Nexus mod.
- **No public UE1 or UE2 render device does stereo**, so there is no per-eye state-tracking
  implementation to copy (same finding as the sibling `unreal-gold-vr` drop).
- **Vireio Perception's `D3DProxyDeviceUnreal`** is the closest relative — its feature list includes
  *“HUD and GUI resizing and 3D depth adjustment”* with the HUD on a secondary render target — but the
  current master is a rewritten v4 whose README says nothing about it, and **the two MTBS3D pages
  documenting the 2.x HUD/GUI depth modes both returned HTTP 403 to automated fetch.** ⚠️ Per our own
  standing rule a 403 is **not** a negative result: this is a live lead someone could open in a browser,
  and it is recorded as unverified rather than absent.
