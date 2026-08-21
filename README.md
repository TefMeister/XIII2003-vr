# XIII (2003) — VR Engine Research

Reverse-engineering research toward a VR conversion of **XIII "Classic"
(2003)**, the classic Unreal Engine 2.x cel-shaded shooter.

This repository holds two things:

- **[`PLAYBOOK.md`](PLAYBOOK.md)** — a reusable, engine-agnostic, point-by-point
  method for taking *any* game whose engine nobody has converted to VR and
  getting it there. It is oriented around one North Star: **the game rendering
  in a headset with head tracking**, with everything else built on top. The same
  playbook is copied into each of our VR projects' research repos.
- **[`ENGINE-DOSSIER.md`](ENGINE-DOSSIER.md)** — the distilled, current-truth
  reference for *this* game's engine: the Unreal-2 module layout, the D3D8
  renderer and its vtable offsets, how the view rotation is overridden for
  head-look, the D3D8→VR frame-capture bridge, the focus-freeze and shutdown
  fixes, the performance profile, and the dead ends that cost us time so they
  don't cost the next engine's.

The blow-by-blow development history lives in the sibling repositories
(`-dev-archive` for the messy in-progress record, `-modding-notes` for readable
field notes). This repo is the consolidated engine knowledge, not the diary.

## Status

Milestone 1 — a framebuffer-in-VR view with live head-look — is **working and
verified in a real Quest 3 headset** (via Virtual Desktop → SteamVR). Current
work is performance and robustness on the capture path. True stereo depth and
6DOF/motion-controlled aim are a deliberately separate future milestone (a
native-ABI approach), because a pure framebuffer capture can never feed VR pose
back into the simulation. See the dossier's status line and open-risks section.

## Scope, ethics, and legality

- This is a **non-commercial fan project**. It requires owning a legitimate copy
  of the game and **redistributes no original game assets** — only files we
  create. See [`.gitignore`](.gitignore).
- The techniques here (DLL proxying, hooking, injection) resemble malware only
  in tooling; the context is personal modding of a game we own.
- We **credit everyone** whose work or research this builds on, and we honour
  correction/removal requests from actual rights holders. See
  [`CREDITS.md`](CREDITS.md).

## Templates

New engine? Start its dossier from
[`templates/per-engine-research-template.md`](templates/per-engine-research-template.md).
