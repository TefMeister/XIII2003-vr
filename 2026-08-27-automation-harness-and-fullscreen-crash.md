# 2026-08-27 — Automation harness (0.2.8) lives; first launch crashed on a FULLSCREEN device re-acquire

Dev PC. Goal for the session: make XIII drivable without a human at the keyboard,
so an unattended session can do real work after the user launches the game.

## What was built

`0.2.8` adds an **automation harness**: console commands are delivered to the
game's own dispatcher from outside the process, with **no simulated input**.

- Commands are appended one per line to `xiii_automation_cmds.txt` next to
  `XIII.exe`. Each tick the proxy reads the file, runs every queued line, and
  truncates it. (Truncation happens *before* execution — so a line still sitting
  in the file proves it never ran. That property paid off within the hour; see
  below.)
- Dispatch is two-tier, mirroring how the real console resolves a command:
  `UObject::ScriptConsoleExec` on the live `APlayerController` for UnrealScript
  exec functions (`god`/`fly`/`ghost`/`walk`), falling back to
  `UGameEngine::Exec` on `GEngine` for engine-level commands.
- `GLog` supplies the `FOutputDevice&` both calls require, so command output
  lands in the game's own log and **no vtable has to be fabricated**.
- Tick site is the existing camera hook (`eventPlayerCalcView`) — game thread,
  once per frame, with a live `APlayerController` already in hand.
- Telemetry (camera position/rotation, throttled) appends to
  `%TEMP%\xiii_capture\xiii_automation.log`.
- Gated behind `[VR] Automation=1`, default off.

### Module split — verified, not assumed

Worth recording, because guessing here fails **silently** (`GetProcAddress`
returns null, the harness just goes inert):

| symbol | module |
| --- | --- |
| `?GEngine@@3PAVUEngine@@A` | **Engine.dll** |
| `?Exec@UGameEngine@@UAEHPBDAAVFOutputDevice@@@Z` | **Engine.dll** |
| `?ScriptConsoleExec@UObject@@UAEHPBDAAVFOutputDevice@@PAV1@@Z` | **Core.dll** |
| `?GLog@@3PAVFOutputDevice@@A` | **Core.dll** |

The first draft looked for all four in `Engine.dll` and would have half-worked.
A raw string scan of the binary is **not** sufficient evidence — export *name
strings* appear in the file either way. These were confirmed by parsing the PE
export directory proper.

Also note the mangling: `PBD` decodes to `char const*`, so this build is **ANSI,
not Unicode** — commands pass as plain `char*`.

### Focus hook extended

The focus hook previously installed only when a VR host was enabled. Automation
needs the engine ticking while unfocused for the same reason a VR host does —
XIII stops calling `Engine->Tick` when it isn't the foreground window, which
would stall the command poll *exactly* when nobody is at the keyboard. It now
installs for `Automation=1` too.

## Live test — partial result, then a crash

**Worked, proven:** the harness initialized cleanly (all four exports resolved)
and telemetry ran for ~95 s of real gameplay — 3,821 ticks, live camera position
logged.

**Not proven:** command execution. The queued `god` was never consumed, because
the user had opened the save menu and **the camera hook does not fire during
menus** — so the tick site goes quiet and the queue stalls. This is a real
limitation of hooking `eventPlayerCalcView` as the tick site, not a one-off.

**The crash**, from `XIII.log`:

```
Log: Data saved in slot 1 took 293 bytes
ScriptLog: END STATE: Transient.InteractionMaster0.XIIIRootWindow0
Log: Viewport WindowsViewport0: WM_DisplayChange
Log: AttemptFullscreen
Log: Enter SetRes: 800x600 Fullscreen 1
Init: Best-match display mode: 800x600x32@75 (Error=0)
Log: Using back-buffer format 22(32-bit)
Log: Using depth-buffer format 75(32-bit)
Critical: UGameEngine::Exec
Critical: UGameEngine::Draw
...
Critical: MainLoop
```

Save → menu exit → display change → fullscreen re-acquire → dead inside the
device re-creation. (`UGameEngine::Exec` at the top of the guard stack is the
*engine's own* `SetRes` console command, not ours.)

**Attribution, honestly: not yet established.** What is known:

- The automation code was **not executing** at crash time — the queue was still
  full, and truncation precedes execution, so it provably never fired.
- The proxy DLL itself is *not* cleared. This was its first run in **fullscreen**;
  every prior session — including the headset-verified home run — was
  **windowed 1280x960**. `ClampWindowedBackbuffer`, the one piece of proxy code
  written for that path, early-returns when `!pp->Windowed`, so it isn't that.
- The dev PC's `XIII.ini` had drifted to fullscreen 800x600 while the deployed
  render device was **stock** (verified by hash: `D3DDrv.dll` was byte-identical
  to `D3DDrv_Original.dll` before this session) — so nothing had ever exercised
  proxy + fullscreen together.

Config is now back on the known-good **windowed 1280x960**. Logs archived to
dev-archive `re-notes/2026-08-27-fullscreen-reset-crash/` (`XIII.log` is
overwritten every launch — copy it before relaunching, always).

## Next

1. Relaunch windowed and re-run the command test — send `god`, then `fly`, and
   watch the telemetry Z climb. That proves injection **from the log alone**, no
   screenshot and no human eyes needed.
2. Decide whether fullscreen is worth supporting at all. It may be simplest to
   document windowed as required, rather than debug a path the VR work never uses.
3. **Consider a second tick site** so the harness keeps working during menus —
   the current one goes quiet exactly when the game is paused, which is a poor
   property for unattended automation. A render-thread `Present` tick would run
   in menus too, but `Exec` must stay on the game thread, so it would need a
   queue handed across.
