# XIII (2003) VR — Alpha 0.1.0

**Render-hook foundation.** This is the groundwork for a VR mod of *XIII - Classic*
(the 2003 Unreal Engine 2 game). It is an early alpha.

> ### ⚠️ Read this first — what 0.1.0 is and isn't
>
> **This build does NOT put the game in a headset yet.** There is no VR output
> in 0.1.0. What it proves is the *plumbing* the VR mod is built on:
>
> - the mod loads into the game without breaking it,
> - it can **capture the game's finished frames**, and
> - it can **steer the in-game camera**.
>
> Everything here is testable on a **normal monitor — no headset needed.** The
> actual "see XIII floating in front of you" step (OpenXR headset output + live
> head-tracking) is the next release, **0.2.0**.
>
> So: you can try this on any Windows PC that runs the game. The Quest doesn't
> come into it until 0.2.0.

---

## What works in 0.1.0

1. **Proxy loads cleanly.** The mod replaces the game's Direct3D render device
   with a pass-through shim that loads the real one underneath, so the game runs
   exactly as before.
2. **Frame capture.** As a built-in demonstration, it writes a handful of
   captured frames to disk (`%TEMP%\xiii_capture\xiii_frame_NNN.bmp`) each run —
   proof the render grab works. (This is a debug demo; it'll become the VR
   frame-submit in 0.2.0.)
3. **Camera injection (optional).** A toggle makes the in-game view slowly pan on
   its own — proof the camera can be driven externally. In 0.2.0 this input
   becomes your real head orientation.

## Requirements

- **XIII - Classic** installed (GOG or Steam; the classic 2003 build).
- Windows.

---

## Install

You are swapping one file in the game's `system\` folder. **Back up the original —
the mod actually needs it.**

1. Open the game's `system\` folder, e.g.
   `...\steamapps\common\XIII - Classic\system\`
2. **Rename** the existing `D3DDrv.dll` to **`D3DDrv_Original.dll`**.
   (This is both your backup *and* the file the mod loads underneath itself.)
3. Copy this release's **`D3DDrv.dll`** into that same `system\` folder.

That's it. Your `system\` folder now has both `D3DDrv.dll` (the mod) and
`D3DDrv_Original.dll` (the untouched original).

## Run the test (monitor only — no headset)

**Test 1 — the game still runs (proves the proxy loads).**
Launch XIII normally. It should reach the menu and play exactly as before. If you
get a "Critical Error" about `D3DDrv.D3DRenderDevice`, you skipped step 2 — make
sure `D3DDrv_Original.dll` exists.

**Test 2 — frame capture.**
Play for ~20–30 seconds, then open `%TEMP%\xiii_capture\`
(paste `%TEMP%\xiii_capture` into the Explorer address bar). You'll find a few
`xiii_frame_*.bmp` screenshots the mod grabbed straight out of the render device.

**Test 3 — camera injection (optional but the fun one).**
1. Open `system\XIII.ini` in a text editor.
2. Add these two lines at the end:
   ```
   [VR]
   CameraSyntheticSweep=1
   ```
3. Launch straight into a level in a window, e.g. from the `system\` folder:
   ```
   XIII.exe DM_Base.unr -windowed -nosound
   ```
   (any map name works; `-windowed`/`-nosound` are just convenient for testing).
4. The view will pan slowly on its own — that's the camera hook steering the
   render camera. The HUD stays put; only the 3D view moves.
5. **To play normally again**, set `CameraSyntheticSweep=0` (or remove the two
   lines).

## Uninstall

1. Delete the mod's `D3DDrv.dll` from `system\`.
2. Rename `D3DDrv_Original.dll` back to `D3DDrv.dll`.

---

## Roadmap

- **0.1.0 (this release)** — render-hook foundation: proxy load, frame capture,
  camera injection. Monitor-verifiable, no headset.
- **0.2.0 (next)** — OpenXR "VR Host": submit captured frames to the headset and
  drive the camera from live head orientation. **This is the release that needs
  a VR headset.**
- **Later** — incrementally replace the flat captured frame with proper
  stereo/spatial presentation; two-handed weapons and roomscale via a deeper
  native integration.

## Notes

- **Always keep `D3DDrv_Original.dll` safe.** It's the real render device; the mod
  cannot run without it, and it's your one-step path back to a clean game.
- Cel-shaded classic *XIII* is a fantastic candidate for VR — this is the first
  step toward getting it there.
