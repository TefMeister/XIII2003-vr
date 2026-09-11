# 2026-09-11 — the self-spin is gone, XIII builds on the dev PC for the first time, and three drops are drained

**`/pd`, dev PC (`DESKTOP-V8GTSIR`), auto-picked. The game was NOT launched. Nothing here has been run.**

---

## 1. ⭐⭐ The game no longer turns on its own — one function, and the caller already handled it

**The defect:** `VrHostGetHeadPose()` returned **`true`** with a synthetic ~12-second-per-revolution
yaw whenever `g_havePose` was false. So with `[VR] CameraLiveHmd=1` and no OpenXR runtime up, the view
turned left at a constant speed for as long as the game ran — seen live on 2026-09-10
`[verified-live 2026-09-10, n=1]`.

**The fix is `return false`, and it is safe because of two things checked rather than assumed:**

1. **There is exactly one caller**, `camera_hook.cpp:70`, and it is already
   `if (VrHostGetHeadPose(...)) { ... }` — so a false return adds nothing to the rotator and the
   game's own camera is left exactly as the trampoline produced it. **Passthrough was already the
   designed behaviour; nothing else needed changing.**
2. **No capability is lost.** The yaw sweep still exists, independently gated behind
   `[VR] CameraSyntheticSweep`, as a *separate* implementation in `camera_hook.cpp`
   (`InterlockedAdd(&s_sweep, kSweepStep)`) that never went through this function at all. The two
   were always different code paths; only one of them was lying about having a pose.

⚠️ **The reason a stand-in had to go rather than be flagged:** the caller cannot tell a synthetic pose
from a real one. Any design where "no data" is returned as plausible-looking data will eventually be
believed by something.

## 2. 🔧 XIII now builds on this machine — and the build script was the reason it could not

`build.bat` **hardcoded** `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\...`, so it
could only ever run on the PC it was written on; this machine has Build Tools on `D:`. It now asks
**vswhere** (Microsoft's own resolver, always at a fixed path) and keeps the old hardcoded path as a
fallback — so it works on both machines instead of one.

**And the link then failed on missing third-party libs**, which turned out to be correct-by-design:
`.lib` is in `.gitignore` and the repo ships `fetch_openvr.md` / `fetch_loader.md` telling you exactly
what to download. Fetched per those instructions, from the sources they name:

- **OpenVR v2.5.1 x86** from Valve's own repo — `openvr_api.lib` (5,578 B), `openvr_api.dll` (639,808 B)
- **OpenXR loader 1.0.10.2 Win32** from the NuGet package — `openxr_loader.lib` (15,638 B),
  `openxr_loader.dll` (1,632,256 B)

⚠️ Both remain gitignored, as intended — this note records **that** they are needed and **where** they
come from, so the next machine costs two commands rather than a puzzle.

**Result: `D3DDrv.dll`, 206,336 B, MSVC 14.44 x86, exit 0** `[compile-verified 2026-09-11]`.

### The export check, and how it nearly produced a false alarm

A proxy that does not forward every export the stock device provides will not load. First comparison
reported *"42 stock exports, 40 ours, zero overlap"* — which would have been alarming and was **an
artefact of my own parsing**: our exports are **forwarders**, so `dumpbin`'s last column is the
forward target (`… (forwarded to D3DDrv_Original.…)`) rather than the name. Comparing the mangled
names properly: **32 unique names on each side, 32 overlapping, none missing**
`[verified-numerically 2026-09-11]`.

⚠️ Worth recording as method: *when a comparison says two things have nothing in common, suspect the
comparison before the things.*

**Size differs from the home PC's 211,968 B M2 build and that is expected twice over** — different
MSVC, and this build has code *removed*.

## 3. 📦 Deployed BESIDE the active device, never over it

This machine's `system\` holds four render devices, and **none of them is the home PC's M2 stereo
build**: `D3DDrv.dll` 275,968 B (active, an unidentified build), `D3DDrv_Original.dll` 417,792 B
(stock), `D3DDrv-m2-recon.dll` 186,880 B. So the 2026-09-04 handoff's "copy the stereo DLL over" was
never completed here.

Ours went in as **`D3DDrv-m2-stereo.dll`** alongside them, following that handoff's own instruction to
place it beside the installed device and never over it. **Stamped for the first time on this machine:**
4 files, `deployed.sh check` ALL MATCH.

⚠️ **The active `D3DDrv.dll` is NOT ours and was not touched.** Nobody should assume the installed
device is the stereo one — it is not, and switching is a deliberate rename.

> ⚠️ **Corrected 2026-09-11f:** the 275,968 B `D3DDrv.dll` **is ours** — byte-identical to
> `staging/XIII2003-vr/D3DDrv-0.2.9.dll`, the 0.2.9 automation-harness build rescued on 2026-09-03
> `[measured 2026-09-11, /lm reader]`. "Not the stereo build" stands; "not ours" was wrong. See
> `2026-09-11f-the-hooks-hold-the-spin-is-gone-and-the-shadow-is-stuck-to-the-screen.md`.

## 4. Three inbox drops drained (dossier §11b, §11c, §11d)

- **🚨 §11b — our device-vtable patches can be silently rewritten by anyone recording a D3D state
  block.** `BeginStateBlock` swaps the **state-setting** methods for recording variants and
  `EndStateBlock` writes the runtime's **own originals** back, over any third-party pointer.
  Non-state-setting methods (`Present`, `Reset`, `Draw*`) are untouched — **so some hooks keep working
  forever and others die permanently, in the same table, silently** `[reported]`. Two independent
  witnesses, one D3D9 (DxWnd's author) and one D3D8. **We are usually not the one recording** — an
  overlay or the engine is, typically re-initialising after a device reset, which is why it reads as
  *"the reset killed my hook"*. On an M2 run it looks identical to two other causes from a screenshot.
  **Queued as a ⭐ `[PD]` row: detect it once per `Present` and log the first mismatched slot.**
- **§11c — VDXR is a *third* OpenXR runtime and no public report covers it.** None of the three
  reports behind §12's risk is about Quest 3 over Virtual Desktop. **Do not expect their signatures
  and do not read their absence as a pass**; the row's positive-identification test stays. Also
  corrects §12's "seven years" — #1253's last activity is **2020-04-22**, not the 2019 creation date.
- **⭐⭐ §11d — the HUD row is really two rows.** The 2D bucket wants a *third* treatment (one tunable
  depth, not zero offset and not the world's), and **the objective marker is world-anchored and in the
  wrong bucket entirely** — fixing the HUD will stop it sliding and leave it at the wrong distance.
  Plus the `D3DFVF_XYZRHW` trap, which decides the implementation and is settled by reading our own
  renderer.

---

## What is NOT established

- **Nothing here has been run.** The spin fix is compile-verified and deployed; no frame has been
  rendered with it, on either machine.
- **That the spin is the only cause of unwanted view motion.** Removing a synthetic yaw removes *that*
  yaw; if the view still drifts with a real pose, the cause is elsewhere and this fix will not have
  addressed it.
- **Whether the built DLL renders correctly.** Matching the stock export set is a **loader** guarantee,
  not a rendering one — the same caveat the 2026-09-04 build carries.
- **Whether the state-block rewrite is happening on this game at all.** §11b is a documented mechanism
  and a plausible risk here; **it has not been observed on XIII**. That is what the queued detector
  would settle.
