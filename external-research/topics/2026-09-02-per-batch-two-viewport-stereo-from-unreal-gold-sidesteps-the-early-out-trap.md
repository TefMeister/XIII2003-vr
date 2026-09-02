# Cross-project: unreal-gold-vr's per-batch two-viewport stereo sidesteps the `SetTransform` early-out trap

**Status:** 🆕 new · **Priority:** high — it is a second architecture for Milestone 2 that makes the
dossier's designed-around trap (§7a) a non-issue, and it was built and numerically verified on a
sibling engine generation today.

## What the sibling did (UE1, `unreal-gold-vr`, 2026-09-02)

`unreal-gold-vr`'s M2 stereo proof `[compile-verified 2026-09-02; 70,550 numeric checks, 0 failures;
never yet rendered]` works like this, in its own render device:

- **every world batch is drawn twice**, once per eye, each through its own **half-width viewport**
  with its own per-eye constants;
- **one shared depth buffer** — the two viewports do not overlap, so the eyes never depth-test
  against each other;
- the **2D layer (menus, HUD, console) stays full-window mono**, drawn after the full viewport is
  restored, so the mouse still maps to the whole window;
- eye offset is a **translation of view space** (UE1 has no view matrix; XIII does — see below);
- off by default, live-switchable from the console (`STEREO 0/1/2`, `IPD`, `SWAPEYES`).

Full write-up: `unreal-gold-vr/modding-notes/2026-09-02-m2-stereo-proof-built-verified-deployed.md`.

## Why it transfers to XIII, and what it changes

XIII's §7a plan hooks **`FD3DRenderInterface::SetTransform`** and sends a modified view/projection
per eye, which runs straight into the engine's **unchanged-matrix early-out**: eye 2's identical
engine matrix compares equal to the cache and is skipped, and stereo collapses silently. The dossier
records the cache-management design as something to "settle before writing the hook".

The per-batch design removes the problem instead of managing it:

1. Hook **one layer down**, at the D3D8 device — `IDirect3DDevice8::SetTransform` (vtable 37) to
   *observe* the engine's `D3DTS_VIEW`/`D3DTS_PROJECTION` as they arrive, and
   `DrawPrimitive`/`DrawIndexedPrimitive` to *draw twice*.
2. For each draw: set viewport L, set `D3DTS_VIEW` = engine view translated by −IPD/2 along the
   camera right axis and `D3DTS_PROJECTION` = the left frustum, draw; set viewport R, the mirrored
   pair, draw; restore.
3. **The engine's `FD3DRenderInterface` cache never sees a modified matrix**, so its compare behaves
   exactly as in stock; the early-out trap is not designed around — it is out of the loop.
4. The 2D/HUD path (UE2 draws tiles through the same interface with an orthographic projection —
   §7a's recon hook already classifies perspective vs orthographic) is left mono, as the sibling did.

Cost: two draws per batch instead of two scene passes (the same batch count the sibling accepted),
and per-eye state churn on a D3D8 device. Both are cheap on this game.

**What is XIII-specific and must be checked:** UE2's D3D8 driver is not purely fixed-function — some
materials go through vertex shaders (hardware skinning, terrain), and for those the view/projection
may arrive as **vertex-shader constants** (`SetVertexShaderConstant`) rather than through
`SetTransform`. §7a's recon log will show whether every draw's transform passed through
`SetTransform`; if some do not, those draws need their constants offset too. This is the one place a
per-draw design can silently miss geometry.

## Prior art that says the layer is sufficient

NVIDIA's stereo driver ran UT2004 (the same UE2 generation, D3D8/D3D9) at an "excellent" community
rating `[reported 2026-09-02]`. That driver worked precisely by intercepting the fixed-function
projection below the engine and rendering each draw for two eyes — the same layer and the same
per-draw doubling as above, done generically. It does not tell us anything about XIII's shaders, but
it is a second, commercial-scale precedent that UE2's transforms are separable per eye **below**
`FRenderInterface`.

## Concrete next steps

1. Run §7a's transform-recon launch as planned — its per-type counts and the perspective/orthographic
   split are exactly the inputs this design needs.
2. Decide between the two architectures on that log: if every world draw's view/projection arrives via
   `SetTransform`, the per-batch D3D8-level design is the simpler one and needs no cache work.
3. Reuse the sibling's numeric test shape (per-eye pixel against an independent projection, both
   layouts, both swap states, zero-IPD identity) — it is engine-agnostic.

## Sources

- `unreal-gold-vr/modding-notes/2026-09-02-m2-stereo-proof-built-verified-deployed.md` (this account, home PC, 2026-09-02)
- https://www.nvidia.com/en-us/geforce/forums/3d-vision/41/116621/cant-get-unreal-tournament-2004-to-work-in-3d-wher/ — UT2004 under NVIDIA's stereo driver
- https://en.wikipedia.org/wiki/Nvidia_3D_Vision — the driver-level D3D stereo mechanism
