# A real native console (F2) exists with Fly/Ghost/God commands — not yet in the dossier, directly useful for Milestone 2 testing

**Status:** ✅ **incorporated** (2026-08-27) · **Priority:** was high — this was a genuine gap in
`ENGINE-DOSSIER.md` (no console was mentioned anywhere in the dossier, including §9's cheat
sheet). It has since become shipped tooling: see [the verdict](#verdict-from-the-modding-side-2026-08-27)
and [the follow-up](#follow-up-2026-08-28--the-open-question-is-now-closed) below.

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

## Verdict from the modding side (2026-08-27)

**The lead was accurate and became shipped tooling.** The modding session built an **automation
harness** (build 0.2.9) driving these same commands from *outside* the process — no F2 keypress,
focus-independent — so an unattended session can drive the game. Verified end to end: `HealMe 100`
written to a text file raised the player's health 50 → 100 in game. The durable version lives in
`XIII2003-vr-engine-research/ENGINE-DOSSIER.md` **§9a**.

### Corrections to the command list above

The list on this page came from a fan wiki. Measured live against the retail Steam build
`[verified-live 2026-08-27]`, it is mostly right with one significant subtraction:

| | commands |
| --- | --- |
| ✅ confirmed present | `God`, `Fly`, `Ghost`, `Walk`, `MaxAmmo`, `HealMe <n>`, `PlayersOnly` |
| ❌ **NOT in this build** | `Teleport`, `AllAmmo`, `Loaded`, `Invisible`, `SetSpeed`, `ChangeSize`, `Slomo` |
| ➕ present, not on this page | `Fov <n>`, `BehindView <0/1>` — these two live on the **PlayerController**, not the CheatManager |

`Suicide` and `KillPawns` were deliberately not tested (destructive).

### Where this page's reasoning was wrong, and it is instructive

Point 1 above argued that because this is the *standard* Unreal cheat-manager surface, other
standard UE1/UE2 commands are plausible candidates. **That was half right, and the failing half
matters:** the standard surface is real, but this build ships a **subset** — six of ten extra
standard commands came back unhandled. *"It is a standard UE command, therefore it is here"* does
not hold. Probe; don't assume.

Also worth knowing for any UE2 title: the cheats are **not** on the PlayerController. They live on
`UCheatManager`, which the console reaches by hopping from the controller — calling
`UObject::ScriptConsoleExec` on the controller alone finds `Fov`/`BehindView` and nothing else.
That distinction cost a debugging round. (Both of these have since been generalised up into
`flat-to-vr-cross-engine-research` → `docs/engines/unreal-1-3.md`.)

## Follow-up (2026-08-28) — the open question is now closed

The verdict asked the research side to chase *"a documented navigation/teleport equivalent, given
`Teleport` is absent — any console-driven way to move the player would be the highest-value
automation primitive still missing."*

**No longer needed: the modding session solved navigation on 2026-08-28 without any teleport
command,** so this is closed rather than open. Per `ENGINE-DOSSIER.md` §9b, movement came from
synthetic **keyboard** input, and the missing piece was never a command at all — XIII *defines*
`TurnLeft`/`TurnRight`/`FastTurnL`/`FastTurnR` aliases and simply **binds none of them to a key**.
Binding spare keys in `DefUser.ini` needed no code change. Measured result `[measured 2026-08-28]`:
~157 uu/s movement, ∓166 °/s turn, closed-loop heading control landing within 0.8°.

Two things that make re-opening this lead a waste of time:

- **Mouse injection is a dead end** `[verified-live 2026-08-28, n=1]` — XIII takes the mouse
  through DirectInput in exclusive mode; 600 px of injected `SendInput` motion produced 0.0° of
  yaw while keyboard input worked in the same session.
- **A console-driven `Button`/`Axis` route is also closed** — those are handled by the viewport's
  `UInput` object, and reaching them via the engine-level `Exec` **crashes the game** (dossier
  §9a, kept as a disproved lead).

`SuperDeform`/`FlowerPower` remain untested and are cosmetic; not worth a session.

## Sources

- https://xiii.wiki.gg/wiki/Cheats_in_XIII_(2003)
- Live verification and the follow-up: our own `XIII2003-vr-engine-research` `ENGINE-DOSSIER.md`
  §9a/§9b (modding session, 2026-08-27 and 2026-08-28).
