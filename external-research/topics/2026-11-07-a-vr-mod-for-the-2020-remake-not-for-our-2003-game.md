# A VR mod exists for the 2020 XIII remake (CastleTroy28's XIII-VR); not for our 2003 game

**Status:** 🆕 new · **Priority:** low. Not a pause trigger (a different game on a different engine), but it is
the first public XIII VR work of any kind, and its feature list is a design reference for ours.

## What is public

Read from the repository's own README, commit list and releases on GitHub (API), 2026-11-07
`[reported 2026-11-07, README + release list]`:

- **Target:** XIII, **the 2020 remake** (64-bit Windows, Steam build tested), not *XIII - Classic* (2003, Unreal
  Engine 2), which is what this project mods.
- **Shape:** a BepInEx 6 plugin (the remake is a Unity IL2CPP game), with a one-click installer and a restore
  script. OpenXR on the system's default runtime (Virtual Desktop named as supported), falling back to OpenVR.
- **Features listed:** tracked hands driving the game's own arms; two-handed long guns; working scopes; manual
  reloads per weapon kind (magazines, shells, revolver, double-barrel, bolt, rockets); body holsters movable by
  laser pointer; physical melee and throwing; doors, keys, keycards and switches by hand; wrist watches carrying
  health, armour and ammo instead of a flat HUD; ladders hand over hand; cutscenes on a still screen (other modes
  selectable); a VR settings and VR controls page added to the pause menu; world-scale setting.
- **State:** "in active development"; seven releases 2026-10-04 to 2026-10-10 (latest v0.1.256, 2026-10-10), no
  commits since 2026-10-10 as of today. Licence marked "Other". 4 stars.

## Why it matters here

1. **Watch rule:** it does not pause `XIII2003-vr` (different game, different engine, nothing reusable at the
   code level). Worth one line on the watch list's "checked" note so a later pass does not re-find it and
   wonder `[inferred-static]`.
2. **Design reference only** (no code is to be read or copied): the wrist-watch HUD, the still-screen cutscene
   mode and the in-menu VR settings page are ideas Tefa may want for the classic game too. The still-screen
   cutscene matches the comfort reasoning our HUD-depth topic already uses `[hypothesis]`.
3. **Hand-smoothing rule check:** the README says weapon "inertia" is applied to guns; whether melee is exempt
   is not stated `[reported]`. Nothing for us to change; noted because our own rule is guns-only.

## Sources

- CastleTroy28, XIII-VR (README, commits, releases): <https://github.com/CastleTroy28/XIII-VR>
