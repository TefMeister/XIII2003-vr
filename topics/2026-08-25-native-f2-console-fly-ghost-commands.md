# A real native console (F2) exists with Fly/Ghost/God commands — not yet in the dossier, directly useful for Milestone 2 testing

**Status:** 🆕 new · **Priority:** high — a genuine gap in `ENGINE-DOSSIER.md` (no console is
mentioned anywhere in the current dossier, including §9's cheat sheet), and directly useful tooling
for the project's current active work (Milestone 2: native-ABI stereo + motion-controlled aim).

## What was found

XIII (2003) ships a real, native, developer-accessible command console on PC, opened with **F2**.
Confirmed command list (PC version specifically):

- `God` — invulnerability (HP/AP frozen regardless of damage)
- `MaxAmmo` — max ammo for held weapons
- `HealMe <n>` — set health (e.g. `HealMe 100`)
- `Suicide` — instant player death
- `KillPawns` — removes all spawned enemies
- `PlayersOnly` — freezes NPCs/vehicles, player still moves
- **`Fly`** — free flight, gravity off, but still respects collision with surfaces
- **`Ghost`** — free flight **with collision disabled entirely** ("nothing hurts you," can pass
  through geometry)
- `Walk` — cancels Fly/Ghost, returns to normal movement
- `Quit`, `SuperDeform` (cosmetic), `FlowerPower` (cosmetic) — less relevant here

## Why this matters for this project specifically

1. **These are stock Unreal Engine 1/2 `GameInfo`/cheat-manager console commands**, not something
   XIII-specific — `God`/`Fly`/`Ghost`/`Walk`/`KillPawns` are the classic Unreal engine cheat-command
   set going back to UE1. This is useful context: it confirms XIII's console exposes the *standard*
   Unreal cheat-manager surface, which means other standard UE1/UE2 console commands not listed on
   this particular fan wiki (e.g. common `Stat`/`Show`/debug-camera commands from the same engine
   family) are plausible candidates worth trying live, even though this research pass didn't find
   them individually documented for this game.
2. **`Ghost` and `Fly` are directly useful for Milestone 2 testing**: free, collision-free camera
   movement through any level is exactly the kind of tool that makes evaluating stereo depth
   correctness and testing motion-controlled aim behavior across varied geometry much faster than
   being constrained to normal gameplay movement — worth using during M2's own visual verification
   passes, the same way this project's Milestone 1 work already used a synthetic yaw-sweep smoke test
   for headset-free verification.
3. **This should be added to `ENGINE-DOSSIER.md` §9** — the current cheat sheet only documents this
   project's own `[VR]` ini keys, with no mention that a native console exists at all. Worth recording
   both the access method (F2) and the command list above as baseline reference.

## Concrete next step

Add the F2 console and its command list to `ENGINE-DOSSIER.md` §9. During Milestone 2 development,
use `Ghost`/`Fly` via the console as a fast, no-injection-required way to explore stereo-depth
correctness and test camera/aim behavior across the game's levels.

## Sources

- https://xiii.wiki.gg/wiki/Cheats_in_XIII_(2003)
