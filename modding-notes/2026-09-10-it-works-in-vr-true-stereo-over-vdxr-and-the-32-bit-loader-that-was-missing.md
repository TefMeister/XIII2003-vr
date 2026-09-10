# 2026-09-10 — it works in VR: true stereo over VDXR, and the 32-bit loader that was missing

Home PC `RTX`, Quest 3 over Virtual Desktop. **OpenXR runtime: Virtual Desktop VDXR, 32-bit**
(`C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr-32.dll` 1.0.10.0,
Streamer 1.34.22.0 — recorded because dossier §12 says the runtime name and version must
travel with any stereo verdict). Tefa driving, three launches.

## Set-up

- `D3DDrv-m2-stereo.dll` (`fcd83c2d2608`, 211,968 B) copied over `D3DDrv.dll`. Stock kept as
  `D3DDrv.dll.bak-2026-09-10-stock` (and `D3DDrv_Original.dll` is still there too).
- `XIII.ini` `[VR]`: `OpenXR=1`, `SteamVR=0`, `OpenXrProjection=1`, `Stereo=1`
  (backup `XIII.ini.bak-2026-09-10-pre-vr`). `CameraLiveHmd=1` was already set.

## Launches 1 and 2 — flat screen, and the view turning left on its own

> XIII is turning left on it's own at a constant speed and not in VR, just a screen in SteamVR

Two causes, both read from the code because the VR host writes its log only through
`OutputDebugString` and nothing reached disk:

1. **No 32-bit OpenXR loader.** XIII is a 32-bit game. `openxr_host.cpp` delay-loads
   `openxr_loader.dll` and disables itself when it is absent. The only loader on this machine
   was SteamVR's `bin\win64\openxr_loader.dll`. The 32-bit *runtime* was fine — the WOW6432 key
   pointed at VDXR's 32-bit json — but with no loader the game never reached it.
2. **The placeholder pose.** `vr_host.cpp:166` `VrHostGetHeadPose()` returns a synthetic yaw,
   one revolution per 12 s, whenever no real pose has arrived, and `camera_hook.cpp:67` adds it
   to the view. That is the constant-speed turn. `[verified-live 2026-09-10, n=1]`

Fix for 1: Khronos loader from NuGet `OpenXR.Loader` 1.0.10.2,
`native/Win32/release/bin/openxr_loader.dll` (1,632,256 B, sha256 `fb1e06de9653…`, PE machine
0x14c), placed beside `XIII.exe`. Fix for 2 is a code change (row on the board).

## Launch 3 — real per-eye VR

> it works now, both eyes line up fine in game but not in the menu. character shadows look
> "spacial and 3d" somehow and hud elements don't line up, like a objective marker that is
> supposed to point exactly what drawer to open, it slides around my view. BUT IT WORKS IN
> VR!!! very awesome. looking left and right feels right, up and down a little off, tilting
> head tilts the world with it and camera is quite shaky

`[verified-live 2026-09-10, n=1 wearer]`:

| reading | what it means |
| --- | --- |
| eyes line up in game | M2 submission + per-eye render both work; **no vertical misalignment on VDXR 1.0.10.0**, so the §12 LukeRoss runtime question is answered for this runtime |
| eyes do NOT line up in the menu | the menu (ortho/2D) is getting the per-eye world offset; 2D must sit at one fixed depth in both eyes |
| HUD elements and the objective marker slide | same rule for the HUD; the marker is a world-anchored 2D element and needs the per-eye projection of its world point |
| character shadows look "spatial and 3D" | a shadow pass getting the eye offset while its receiver does not (or vice versa); classify the bucket in the stereo log |
| yaw right | `camera_hook.cpp:75` sign and scale are right |
| pitch a little off | converted without the engine's own pitch handling; compare in the log |
| roll tilts the world | roll is never applied (`:75-78` do yaw and pitch only) |
| camera shaky | pose read on the worker thread, consumed in `PlayerCalcView` with no display-time prediction; plus 24–72 fps swings in the stereo log |

Stereo log (`%TEMP%\xiii_capture\xiii_stereo.log`) for the two failed sessions: `fov=engine`
(views never located), `mono-ortho=26.0` draws/frame in game — the 2D bucket the HUD fix will
use.

## State left on this machine

The stereo driver is now the **active** `D3DDrv.dll` here, `[VR]` is set for OpenXR, and the
loader is beside the exe; `deployed/RTX/XIII2003-vr.tsv` re-stamped. ⚠️ Until the placeholder
pose is removed, launching XIII **without** Virtual Desktop running will spin the view.
