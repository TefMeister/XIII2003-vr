# 2026-08-27 (same day, later) — the automation harness WORKS, verified in game

Follow-up to [`2026-08-27-automation-harness-and-fullscreen-crash.md`](2026-08-27-automation-harness-and-fullscreen-crash.md).
XIII can now be driven from outside the process. **The user witnessed
`HealMe 100` raise their health 50 → 100**, from a command written to a text
file — the end-to-end proof the earlier entry was missing.

## Root cause of the 0.2.8 crash: the dispatch site

0.2.8 GPF'd the moment a command was delivered:

```
History: UGameEngine::Exec <- UGameEngine::Draw <- UWindowsViewport::Repaint
         <- UWindowsClient::Tick <- ClientTick <- UGameEngine::Tick <- ...
```

The queue was drained from the camera hook (`eventPlayerCalcView`), and that
stack shows the camera hook is reached **from inside `UGameEngine::Draw`**.
Console commands were being executed re-entrantly mid-render.

Three pieces of evidence agreed before a line was changed: telemetry stopped on
the exact tick the command was consumed, the process fell to **0% CPU**, and
nothing the command should have printed ever reached `GLog`.

**Fix:** dispatch from `APlayerController::Tick` — game-logic phase, outside the
render path. Prologue `55 8B EC 6A FF` is exactly 5 bytes with no relative
operands, so the trampoline is clean; it is prologue-verified and fails safe.
Moving the call site fixed it outright, first try.

## Second finding: the cheats are on a different object

With the crash gone, probing became cheap and safe — which immediately paid off:

```
Fov 90       -> playercontroller      God/Fly/Ghost/Walk  -> unhandled
BehindView 1 -> playercontroller      Teleport/MaxAmmo/…  -> unhandled
```

Dispatch was working; the commands were reaching the wrong object. Every
unhandled name is a classic UE2 **`UCheatManager`** exec function. The console
reaches them by hopping from the controller to its CheatManager;
`UObject::ScriptConsoleExec` on the controller does not.

**Located by identity, not offset:** `??_7UCheatManager@@6B@` (its vtable) is
exported, so the harness scans the controller's fields for a pointer whose
target's vtable is exactly that. Found at `controller+0x598`. This beats
hardcoding an offset we could not verify, is self-validating, and survives
field reordering. Re-validated on use — the CheatManager dies on level change.

## The command map for this build

| resolves on | commands |
| --- | --- |
| `APlayerController` | `Fov <n>`, `BehindView <0/1>` |
| `UCheatManager` | `God`, `Fly`, `Ghost`, `Walk`, `MaxAmmo`, `HealMe <n>`, `PlayersOnly` |
| absent from this build | `Teleport`, `AllAmmo`, `Loaded`, `Invisible`, `SetSpeed`, `ChangeSize`, `Slomo` |

`Teleport` being absent is the one that stings — it would have been the best
automation primitive for navigation. `Suicide`/`KillPawns` deliberately untested.

## Hardening carried in the same fix

Because the faulting call could **not** be isolated from 0.2.8's logs:

- **BEGIN/END logging around every call, flushed.** 0.2.8 logged only on
  completion, so its crash left no fingerprint and had to be inferred. Now a
  fault names the command and the tier in flight. This is the single change
  that would have saved the most time, and it is cheap.
- The `FOutputDevice` is validated (committed, readable, plausible vtable)
  before the engine dereferences it blindly.
- `UGameEngine::Exec` — the tier that actually faulted — is now **opt-in**
  (`AutomationEngineExec`, default 0). The default path never touches it.
- Re-entrancy guard on the drain.

## Corrected from the earlier entry

That entry suspected the **fullscreen** device re-acquire, because the dev PC
had drifted to fullscreen while all prior work was windowed. That was a red
herring — both crashes shared the same guard stack, and the common factor was
`UGameEngine::Exec`, not the display mode. Windowed 1280x960 is restored anyway
(it is the known-good config), but it was not the cause.

## Session result

~20 commands executed across two launches, no faults, clean process exits (no
`Critical:` frames). Game closed gracefully via `WM_CLOSE`, never force-killed.
Game state left as found: `God` toggled back off, movement ended on `Walk`,
`PlayersOnly` toggled back, `BehindView` restored.

## Next

1. **Navigation is the open problem.** With `Teleport` absent, there is no
   console-driven way to move the player. Options: find an XIII-specific
   equivalent, or write the position directly (the telemetry already reads the
   camera location, so the address is in hand).
2. Telemetry only logs position/rotation — health/state changes are invisible
   to it, which is why the `HealMe` proof needed human eyes. Worth extending.
3. The render-path lesson is engine-agnostic and has been sent to
   `flat-to-vr-cross-engine-research/inbox/`.
