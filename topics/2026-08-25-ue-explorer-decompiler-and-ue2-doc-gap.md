# UE Explorer is the confirmed right tool for the Milestone 2 UnrealScript decompilation lever — but no UE3-style public engine documentation exists for UE2

**Status:** 🆕 new · **Priority:** high — directly targets the dossier's own explicitly-named
Milestone 2 lever ("gameplay-side RE is far cheaper than native disassembly — a lever not yet
pulled") and sets honest expectations for the native camera/projection side of M2.

## The tool: UE Explorer

**[UE Explorer](https://github.com/UE-Explorer/UE-Explorer)** (by EliotVU, actively maintained, C#/
.NET WinForms) is the standard, well-established community tool for exactly the job
`ENGINE-DOSSIER.md` §2 already flags as available but unused: decompiling `.u`/`.upk` UnrealScript
package files back to near-source `.uc`. Confirmed feature set directly relevant here:

- Explicitly supports **Unreal Engine 1, 2, 2.5, and 3** package formats — covers XIII's UE2.x
  packages (`xiii.u`, `gameplay.u`, `xidpawn.u`, etc., already identified in the dossier) without
  ambiguity about compatibility.
- Exports decompiled classes to `.uc` (readable UnrealScript source), and separately offers
  **bytecode token-level output** for cases where the decompiled source isn't quite enough — useful
  if a particular function's decompilation is ambiguous or fails, since the raw token stream is still
  inspectable.

## Why this matters for Milestone 2 specifically

The dossier's own §12 (open risks) identifies the core M2 problem: **"6DOF/motion-controlled two-handed
weapon aim requires feeding VR pose into the simulation, which the framebuffer approach structurally
cannot do... the UnrealScript `*.u` bytecode being decompilable is the lever."** UE Explorer is the
concrete tool to pull that lever: decompiling `xidpawn.u` (and whichever package owns weapon-handling/
aim logic) should reveal the actual UnrealScript-level functions that read player aim/view rotation
and drive weapon orientation — the natural targets for injecting VR-derived pose data into the
simulation, rather than only overriding the final render-side rotation the way Milestone 1's
`eventPlayerCalcView` hook does.

## A related, honest gap: no UE3-style public engine documentation exists for UE2

Worth setting expectations precisely: this portfolio's Alice: Madness Returns front (a UE3 project)
benefited from substantial official public UE3/UDK documentation (Epic's own Camera Technical Guide,
forum threads on shader constant registers, etc.) because Epic later released UDK with real
documentation. **No equivalent exists for UE2** — checked directly: Unreal Tournament 2004 (the same
UE2.x generation, Epic/Digital Extremes' own flagship UE2 title) never got a comparable public SDK
release with documented render-device source; its own D3D9 renderer path is described by the
community as having been left "experimental and functionally incomplete" by Epic. **This means
Milestone 2's native camera/projection work (finding how UE2's D3D8 renderer receives the
view/projection transform, analogous to what public UE3 docs gave Alice for free) will need to be
derived from this project's own live disassembly/shader-reflection work, not looked up** — the same
discipline Milestone 1's own `eventPlayerCalcView` hook and D3D8 vtable work already used
successfully, just extended to cover per-eye projection math rather than only view rotation.

## Concrete next step

When Milestone 2 work begins, use UE Explorer to decompile the packages already identified in the
dossier (`xidpawn.u` first, as the most likely owner of pawn/weapon-aim logic) and look specifically
for the UnrealScript functions that compute aim direction/weapon orientation each frame — these are
the natural injection points for VR pose data, complementing (not replacing) the render-side work
already done for Milestone 1's head-look. Don't expect to find equivalent public UE2 rendering
documentation the way Alice's UE3 front could — budget for this project's own live D3D8
shader-reflection work instead.

## Sources

- https://github.com/UE-Explorer/UE-Explorer
- https://eliotvu.com/portfolio/ue-explorer
- https://www.pcgamingwiki.com/wiki/Unreal_Tournament_2004 (via search-engine summary; D3D9 renderer left experimental/incomplete by Epic)
