# §7a alternative: draw each batch twice at the D3D8 level (unreal-gold's M2 design) and the early-out trap is out of the loop

Filed by: `/gr`, 2026-09-02
Topic: `external-research/topics/2026-09-02-per-batch-two-viewport-stereo-from-unreal-gold-sidesteps-the-early-out-trap.md`
Dossier section: §7a ("The trap: the unchanged-matrix early-out")
Cross-project source: `unreal-gold-vr/modding-notes/2026-09-02-m2-stereo-proof-built-verified-deployed.md` `[compile-verified 2026-09-02; verified-numerically 2026-09-02, 70,550 checks]`

unreal-gold-vr's M2 draws **every world batch twice**, each eye through its own half-width viewport with its own per-eye constants, one shared depth buffer, 2D/HUD left full-window mono, off by default and console-switchable. Applied to XIII: hook `IDirect3DDevice8::SetTransform` (vtable 37) to *observe* the engine's view/projection and `Draw(Indexed)Primitive` to draw twice with per-eye `D3DTS_VIEW`/`D3DTS_PROJECTION` and viewports. **`FD3DRenderInterface`'s cache never receives a modified matrix**, so the compare behaves as stock and there is nothing to design around.

Check before choosing it: §7a's recon log must show every world draw's transform arriving via `SetTransform`. UE2's D3D8 driver uses vertex shaders for some materials (skinning, terrain); those may receive view/projection as **vertex-shader constants** and would need offsetting separately — the one way a per-draw design silently misses geometry.

Prior art at the same layer: NVIDIA's stereo driver ran UT2004 (same UE2 generation) at an "excellent" rating by intercepting the fixed-function projection per draw below the engine `[reported 2026-09-02]`.

Suggested dossier change: add this as the second route beside the `FCameraSceneNode` fallback in §7a, with the recon log as the deciding input.
