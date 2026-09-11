# Credits & Attribution

This project is a reverse-engineering and modding effort built on the public
research, tools, and creative work of many people who came before us. None of
this would be possible without them. We list every source, tool, and prior
work we have drawn on below — by name or handle, as accurately as we could
verify it — including those that helped only as inspiration.

If we have missed someone, the omission is a mistake, not a slight. Please see
the "Get credited, or ask us to stop" section at the bottom.

## The original game

XIII (2003) is the creative work of its developer and publisher. We are only
modding it; we did not make it, and all rights to the game and its assets
belong to their owners. No game files are included in any repository in this
project.

| Work | Creator(s) | Note |
|---|---|---|
| XIII (2003), original "Classic" game | Ubi Soft Paris (developer); Ubi Soft (publisher) | Cel-shaded FPS adapting the Belgian comic *XIII* by Jean Van Hamme & William Vance. |
| Unreal Engine 2.x (the base engine) | Epic Games | The engine XIII is built on; source of the RenderDevice plugin model and the `FRotator` convention. |

## Prior art, tools, and research this repo draws on

This is a new repo (seeded 2026-08-24) for public-research leads specifically —
see [XIII2003-vr-modding-notes](https://github.com/TefMeister/XIII2003-vr/tree/main/modding-notes)'s own
`CREDITS.md` for the full list of tools/prior-art the mod itself already draws on. This table
grows as `/game-research XIII2003-vr` finds new leads.

| Source / Work | Creator(s) | Link |
|---|---|---|
| XIII (2003) console commands documentation | xiii.wiki.gg community | https://xiii.wiki.gg/wiki/Cheats_in_XIII_(2003) |
| UE Explorer | EliotVU | https://github.com/UE-Explorer/UE-Explorer |
| PCGamingWiki (Unreal Tournament 2004 D3D9 renderer notes) | PCGamingWiki community | https://www.pcgamingwiki.com/wiki/Unreal_Tournament_2004 |
| Unreal Developer Network archive — the public Unreal Engine 2 documentation set (UnrealScript reference, runtime topics, `UnDox`, and the `RuntimeHeaders` introduction naming "360 degree rendering drivers for VR systems") | Epic Games | https://docs.unrealengine.com/udk/Two/RuntimeHeaders.html |
| vorpX community forum (the UT2004 OpenGL-renderer requirement, cited as a UE2-family data point) | vorpX forum contributors | https://www.vorpx.com/forums/topic/unreal-tournament-2004/ |
| XIII UnrealScript source mirrors (2005 leak of the Xbox branch, uploaded by Sonrat; read online, study-only, nothing copied — rights holder Ubisoft) | artism90, Ch0wW, VideogameSources (mirrors) | https://github.com/artism90/xiii-unrealscript · https://github.com/Ch0wW/xiii_unrealscript · https://github.com/VideogameSources/XIII |
| UT2004 reverse-engineered native headers thread | OldUnreal community | https://www.oldunreal.com/phpBB3/viewtopic.php?t=10021 |
| UnrealScript source availability notes (XIII packages stripped) | Beyond Unreal wiki (archive) | https://beyondunrealwiki.github.io/pages/unrealscript-source.html |
| UT2004 under NVIDIA's stereo driver (D3D8-era per-draw stereo precedent) | NVIDIA GeForce forums community | https://www.nvidia.com/en-us/geforce/forums/3d-vision/41/116621/cant-get-unreal-tournament-2004-to-work-in-3d-wher/ |
| OpenXR SDK — `openxr.h` composition-layer struct definitions (read online, described in our own words, nothing copied) | The Khronos Group | https://github.com/KhronosGroup/OpenXR-SDK |
| OpenVR issue #1253 — the per-eye pose defect, re-checked 2026-09-02 | LukeRoss00 (reporter), ValveSoftware/openvr | https://github.com/ValveSoftware/openvr/issues/1253 |

AI development assistance: **Claude (Anthropic)** (https://www.anthropic.com).

Project lead and author: **TefMeister**.

## Missing from this list?

If you — or someone whose work you know — contributed to, influenced, or
even just inspired anything used in this project and you aren't credited
here, please **open a GitHub issue on this repo** and we'll correct it as
soon as possible. We would much rather over-credit than leave anyone out.

## Added by the 2026-09-11 research pass

For the HUD-depth and stereo-bucket findings behind
`topics/2026-09-11-the-hud-needs-a-third-treatment-and-the-objective-marker-is-in-the-wrong-bucket.md`:

- **NVIDIA GameWorks** — the archived *3D Vision Automatic* best-practices documentation, which is still
  the clearest written statement of the separation/convergence technique and of the rule that 2D must be
  drawn at convergence depth while world-referenced HUD takes its object's depth.
- **bo3b (Bo3b Johnson)** — the 3Dmigoto wiki and *School for Shaderhackers*, for the canonical stereo
  formula and for *Auto Crosshair*, the published solution to finding a world-anchored element's depth
  per frame.
- **DarkStarSword** — `3d-fixes` and its `shadertool` / `hlsltool` / `asmtool` UI-depth insertion, and
  the auto-HUD toggles in his Far Cry 4 and Akiba's Trip fixes.
- **Helix**, **Losti**, **DHR** and the HelixMod community — for the HUD-depth and crosshair-depth key
  conventions that made "expose it, don't hard-code it" the obvious design.
- **"admin" at xdPixel** — *Decoding a Projection Matrix*, for the orthographic-vs-perspective test.
- **Microsoft** — the Direct3D fixed-function FVF documentation, which is where the `D3DFVF_XYZRHW`
  pre-transformed-vertex trap comes from.
- **GhwstVR** — *UT99 Quest*, for the quad 2D layer; **the Khronos OpenXR Working Group** for
  `XrCompositionLayerQuad`.
- **cybereality / Denis Reischl** and contributors — *Vireio Perception*, whose `D3DProxyDeviceUnreal`
  remains the closest relative to this project's patched device, recorded as an unverified lead.

## Respecting creators

This project exists because other people generously shared their
reverse-engineering research, tools, and modding know-how in public — we've
tried to credit every one of them by name or handle above, as accurately as
we could verify. If you are the creator or rightful owner of anything
credited or used here and you'd rather your work not be referenced in this
repo, or you want specific content removed or no longer used by the mod,
please tell us: **open a GitHub issue on this repo**. We'll act on that
request promptly — no argument, no delay — and we'll find another way to get
the job done that doesn't rely on your material. This is your work; we're
just grateful to have learned from it.
