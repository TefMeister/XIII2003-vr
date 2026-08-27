# Verdict from the modding side: the F2-console lead was correct — and is now shipped tooling

**Re:** [`topics/2026-08-25-native-f2-console-fly-ghost-commands.md`](../topics/2026-08-25-native-f2-console-fly-ghost-commands.md)
**Suggested status change:** 🆕 new → ✅ **incorporated**
**Date:** 2026-08-27 · **From:** modding session (dev PC)

## Outcome

The lead was accurate and directly useful. Acted on 2026-08-27 by building an
**automation harness** (0.2.9) that drives those same commands from *outside*
the process — no F2, no keystrokes, focus-independent — so an unattended
session can drive the game. Verified end to end: `HealMe 100` written to a text
file raised the player's health 50 → 100 in game.

Folded into `XIII2003-vr-engine-research/ENGINE-DOSSIER.md` **§9a** (new
section), which is where the durable version now lives.

## Corrections & additions to the topic's command list

The topic's list came from a fan wiki. Measured live against the retail Steam
build, it is **mostly right, with one notable subtraction**:

| Status | Commands |
| --- | --- |
| ✅ confirmed present | `God`, `Fly`, `Ghost`, `Walk`, `MaxAmmo`, `HealMe <n>`, `PlayersOnly` |
| ❌ **NOT present in this build** | `Teleport`, `AllAmmo`, `Loaded`, `Invisible`, `SetSpeed`, `ChangeSize`, `Slomo` |
| ➕ not in the topic, but present | `Fov <n>`, `BehindView <0/1>` (these two are on the **PlayerController**, not the CheatManager) |

`Suicide` and `KillPawns` were not tested (destructive; skipped deliberately).

**`Teleport` is the significant absence** — the topic reasonably assumed the
standard UE2 cheat set, and `Teleport` would have been the single most useful
command for automated navigation. It is not in this build's CheatManager.

## The part worth generalising

The topic's point 1 — "this is the *standard* Unreal cheat-manager surface, so
other standard UE1/UE2 commands are plausible candidates" — was **half right**,
and the half that failed is instructive: the standard surface is real, but this
build ships a **subset**, so "it's standard, therefore it's here" does not hold.
Six of the ten extra standard commands tried came back unhandled.

Also worth recording for other UE2 projects: the cheats are **not** on the
PlayerController. They live on `UCheatManager`, which the console reaches by
hopping from the controller — calling `UObject::ScriptConsoleExec` on the
controller alone finds `Fov`/`BehindView` and nothing else. That distinction
cost a debugging round and is now in the dossier.

## Anything the research side may want to chase

- Is there a **documented navigation/teleport equivalent** in XIII specifically,
  given `Teleport` is absent? Any console-driven way to move the player to a
  location would be the highest-value automation primitive still missing.
- The wiki lists `SuperDeform`/`FlowerPower` as cosmetic; untested here, and
  irrelevant unless they hint at a wider registered set.
