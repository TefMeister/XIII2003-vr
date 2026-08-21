# XIII (2003) VR — Alpha 0.2.0

**First headset build (experimental).** This is a VR mod of *XIII - Classic*
(the 2003 Unreal Engine 2 game). 0.2.0 adds the first attempt at putting the
game into a VR headset — a flat "framebuffer in VR" view with live head-look —
on top of the 0.1.0 render-hook foundation.

> ### ⚠️ Read this first — 0.2.0 is experimental and headset-UNTESTED
>
> This VR output has **only been verified on a monitor** (it builds, loads, and
> captures frames correctly, and the head-look math is unit-tested). Whether a
> frame actually reaches the headset, and whether head-look feels right, is
> exactly what this build is for **you** to try. Expect rough edges.
>
> **Your game is never at risk.** Everything is gated **off by default**, so
> with no configuration the game runs completely normally on your monitor.
>
> If it doesn't work, that's useful information — please report what you see
> (Contact, below).

---

## The five repositories for XIII (2003) VR

Everything for this game lives in five repositories, each with one job — so you
always know where to look. You are in **XIII2003-vr-mod**.

| Repository | What lives here |
| --- | --- |
| **XIII2003-vr-mod** ← you are here | The mod itself — the VR head-look `D3DDrv.dll` render-device proxy. |
| [XIII2003-vr-dev-archive](https://github.com/TefMeister/XIII2003-vr-dev-archive) | Full development history — snapshots, probes, dead ends, raw recon. |
| [XIII2003-vr-modding-notes](https://github.com/TefMeister/XIII2003-vr-modding-notes) | Readable field notes / progress ledger. |
| [XIII2003-vr-staging](https://github.com/TefMeister/XIII2003-vr-staging) 🔒 | **Private** — unverified WIP builds, cross-machine handoff. |
| [XIII2003-vr-engine-research](https://github.com/TefMeister/XIII2003-vr-engine-research) | Distilled engine reference (dossier) + reusable VR RE playbook. |

## Download

Grab **`XIII2003VR-0.2.0-alpha.zip`** from the
[**Releases page**](https://github.com/TefMeister/XIII2003-vr-mod/releases).
It contains the mod (`D3DDrv.dll`), Valve's `openvr_api.dll`, and a
`READ_ME_FIRST.txt` with the same steps below.

## Requirements

- **XIII - Classic** installed (GOG or Steam; the classic 2003 build).
- A VR headset. For a **Quest 3**, this is reached over **Virtual Desktop**
  (see the two runtime options below).
- Windows.

---

## Install

You swap **one** file and add **one** file, both in the game's `system\` folder.

1. Open the game's `system\` folder, e.g.
   `...\steamapps\common\XIII - Classic\system\`
2. **Rename** the existing `D3DDrv.dll` to **`D3DDrv_Original.dll`**.
   (This is both your backup *and* the file the mod loads underneath itself.)
3. Copy the release's **`D3DDrv.dll`** into that same `system\` folder.
4. Copy the release's **`openvr_api.dll`** into that same `system\` folder.

This mod ships **no original game files**; you must own a legitimate copy of XIII.

## Choose a VR runtime — enable exactly ONE

Open `system\XIII.ini` and add a `[VR]` section at the end.

### Option A — SteamVR (recommended, try this first)

**Runtime:** the **SteamVR** runtime, installed and launched through Steam.

```ini
[VR]
SteamVR=1
CameraLiveHmd=1
```

- Start SteamVR before (or just after) launching the game.
- On a **Quest 3**, reach SteamVR through **Virtual Desktop**: run the Virtual
  Desktop streamer on the PC, connect the headset, and launch/stream **SteamVR**
  from inside Virtual Desktop. (Quest Link / Air Link into SteamVR also works.)
- Needs `openvr_api.dll` (included in the release zip).

### Option B — OpenXR (alternative)

**Runtime:** an **OpenXR** runtime — **not** SteamVR. On a Quest 3 over Virtual
Desktop this is **VDXR**; Oculus's OpenXR runtime works too.

```ini
[VR]
OpenXR=1
CameraLiveHmd=1
```

- Needs `openxr_loader.dll` next to `D3DDrv.dll`. It is **not** in the release
  zip (separate redistributable). Only use Option B if you already have it;
  otherwise use Option A.

> Enable **exactly one** of `SteamVR` / `OpenXR`. Never both.

## Run

With your chosen runtime started and the flag set, launch the game (a level
loads faster than the menu for a quick test), from `system\`:

```
XIII.exe DM_Base.unr -windowed -nosound
```

**What you should see:** a flat screen showing the game, floating in front of
you and locked to your head. Turning your head rotates the in-game camera, so
you look around the game world by looking around. (The screen staying centred is
expected — the head-look happens in the game view, not by the screen moving.)

If colours look swapped (red/blue), that's a known one-line knob on our side —
just let us know and we'll flip it.

## Turn it off / uninstall

- **Play normally again:** set `SteamVR=0` (and `OpenXR=0`), or remove the `[VR]`
  section.
- **Fully uninstall:** delete the mod's `D3DDrv.dll` and `openvr_api.dll` from
  `system\`, then rename `D3DDrv_Original.dll` back to `D3DDrv.dll`.

## Diagnostics

The mod logs through `OutputDebugString`. Run **DebugView** (as administrator,
with *Capture Global Win32* enabled) to see `[xiii-steamvr]` / `[xiii-openxr]`
lines — they show how far the VR bring-up got if nothing appears.

---

## Roadmap

- **0.1.0** — render-hook foundation: proxy load, frame capture, camera
  injection. Monitor-verifiable, no headset.
- **0.2.0 (this release)** — first headset build: flat framebuffer in VR via a
  head-locked overlay/quad layer, plus live head-look, on **SteamVR** or
  **OpenXR**. Experimental; awaiting real-headset validation.
- **Later** — replace the flat captured frame with proper stereo/spatial
  presentation; two-handed weapons and roomscale via a deeper native
  integration.

## Notes

- **Always keep `D3DDrv_Original.dll` safe.** It's the real render device; the
  mod cannot run without it, and it's your one-step path back to a clean game.
- Cel-shaded classic *XIII* is a fantastic candidate for VR — this is the mod
  getting its first look through the headset.

---

## Credits & attribution

This is an unofficial, non-commercial fan mod, made possible by the work of many:

- **Ubisoft** — creators and rights holders of *XIII* (2003). All rights to the
  game and its assets belong to them. This mod ships **no original game files**
  and requires you to own a legitimate copy of the game.
- **Valve** — for the [OpenVR / SteamVR SDK](https://github.com/ValveSoftware/openvr)
  (`openvr_api.dll` is redistributed here under its BSD-3-Clause licence), and
  the **OpenXR** ecosystem for the alternative runtime path.
- **The Unreal Engine 1/2 modding community** — for decades of shared knowledge
  about the engine's render devices, native-class registration, and the FRotator
  rotation convention.
- **Open-source tools** that made the work possible, including
  [x64dbg](https://x64dbg.com/), [doctest](https://github.com/doctest/doctest),
  and [pefile](https://github.com/erocarrera/pefile).
- **Everyone** whose tutorials, forum posts, projects, or ideas provided
  inspiration along the way — whether named here or not.

If you helped in any way, even just as inspiration, and feel you should be
credited but aren't, please email us and we will add you as soon as possible.

## Corrections & removal requests

- **Missing or incorrect credit?** Email us and we will correct it as soon as
  possible.
- **Removal requests:** if you are the owner or creator of something used in this
  mod and you want it removed from the mod or this repository, please contact us.
  We will honour legitimate requests from rights holders promptly.

**Contact:** td3kxlvr@proton.me
