# Milestone 1: VR Head-Look — Design

**Status:** Draft, pending review
**Goal:** Look around in VR while playing XIII - Classic on a Quest 3 (via Virtual Desktop), as the first proof point on the way to full roomscale 6DoF + motion-controlled two-handed weapons.

## Context

XIII - Classic (GOG build, no Steam DRM/anti-cheat) runs on a classic Unreal Engine (2.x-era) architecture:

- `XIII.exe` — thin launcher
- `Core.dll` / `Engine.dll` — simulation, native engine code
- `D3DDrv.dll` — swappable render device (Unreal's pluggable `RenderDevice` interface)
- `WinDrv.dll` — window/input driver
- `xiii.u`, `gameplay.u`, `xidpawn.u`, etc. — gameplay logic as compiled UnrealScript bytecode (likely decompilable to near-source, lowering RE burden versus native disassembly)

This project has two documented target strategies (see the user's guides repo, `Brobert-in-aus/guides`):
- `vr/legacy-framebuffer-to-spatial-vr.md` — host game unmodified, capture finished framebuffer at present, show it in VR, incrementally replace with structured presentation.
- `vr/native-abi-godot-vr.md` — wrap simulation behind a versioned C ABI, let Godot own OpenXR input/presentation/platform integration.

**Decision:** pursue both as staged milestones of one project, not competing strategies. Milestone 1 (this doc) is a legacy-framebuffer-style head-look proof. Milestone 2 (future) moves toward native-ABI so OpenXR hand/head poses can drive gameplay-affecting camera/weapon aim for true roomscale 6DoF and two-handed weapons — full head/gamepad look-around cannot get there, because the simulation never sees VR pose data under a pure framebuffer approach. Milestone 1's hook work (render-device proxy, discovery of the present call) is reusable groundwork for Milestone 2, not throwaway.

**Test environment split:** dev machine (this one) has no headset attached — all VR runtime testing happens on a separate machine at home with a Quest 3 over Virtual Desktop (VDXR OpenXR runtime, avoids requiring Meta/Oculus PC software).

## Architecture

XIII keeps running as a normal single process — no process split (that's Milestone 2 territory).

Code gets into the process via a **proxy `D3DDrv.dll`**: Unreal's render device is a pluggable DLL loaded by name from `Engine.dll`. We ship our own `D3DDrv.dll` that loads the real one under a renamed filename, forwards all calls through unmodified, and intercepts only the present/frame-complete call. This is idiomatic for the engine — no external injector needed, the game loads our code just by us dropping a file in `system/`.

The proxy owns an **OpenXR session** (instance, swapchain, per-frame Wait/Begin/EndFrame, HMD pose polling) — generic OpenXR code, not XIII-specific, and potentially reusable outside this project.

A second, separate hook point overrides camera yaw/pitch with HMD orientation — likely native engine code (not the render device), so it needs its own RE pass to locate, and a lightweight inline hook (not a live debugger) to override at runtime.

## Components

1. **VR Host module** — OpenXR instance/session/swapchain lifecycle, per-frame pose polling and layer submission. Generic, reusable.
2. **Render-Device Proxy (`D3DDrv.dll` shim)** — loads the real D3DDrv under a renamed file, forwards its interface, intercepts present to trigger the VR Host's per-frame submit. Also keeps forwarding the real present so the desktop window still updates (useful for dev-machine testing without a headset).
3. **Camera-Injection Hook** — inline hook on the native view-rotation update function; overrides yaw/pitch with HMD pose when VR is active.
4. **Config/toggle** — ini or env-var gate to disable the VR path entirely, so normal desktop play is never at risk.

## Data Flow (per frame)

```
game tick
  -> camera/view update (hooked: yaw/pitch overridden from HMD pose, if VR active)
  -> game renders scene through D3DDrv proxy (passthrough to real D3DDrv)
  -> proxy intercepts present
      -> VR Host grabs the rendered frame
      -> submits to OpenXR swapchain as a same-image-both-eyes layer (no true stereo depth yet — original renderer is monoscopic)
      -> compositor displays in headset
  -> proxy also forwards the real present, so the desktop window keeps updating
```

## Error Handling

Fail closed, always:
- OpenXR init failure (no runtime active, no headset, Virtual Desktop not running) → VR path disables itself, game behaves exactly like the unmodified install.
- Camera-injection hook has its own config toggle, independent of the render hook.
- Bad/stale HMD pose data (NaN, stale frame) → skip the override for that frame rather than writing garbage into the camera.
- Nothing in this design should be able to prevent normal, unmodified play.

## Testing

Split across two machines:

- **Dev machine (no headset):** proxy DLL loads and forwards correctly; game runs unaffected with VR disabled (regression safety); a synthetic "smoke test" mode feeds a scripted sweeping yaw/pitch (not real HMD pose) so the camera-injection hook can be visually verified from the desktop window alone, without a headset attached.
- **Home machine (Quest 3 + Virtual Desktop):** real OpenXR session; verify axis correctness (no reversed/swapped yaw-pitch), tracking latency feel, general comfort.

## Out of Scope (Milestone 1)

- True stereo depth / separate eye rendering
- Motion-controlled weapon aim / two-handing
- Roomscale positional (6DoF) movement
- Any change to gameplay simulation beyond camera yaw/pitch override

These are Milestone 2+ (native-ABI) work.
