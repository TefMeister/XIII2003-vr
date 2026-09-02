# XIII's UnrealScript source is public — the Milestone 2 aim lever is a read, not a decompile

**Status:** 🆕 new · **Priority:** ⭐ high — it replaces the dossier's named M2 lever ("decompile the
`.u` packages to find the weapon-aim functions") with something cheaper, and it already answers the
first question that lever was going to ask.

## What was found

The original UnrealScript source of XIII exists on GitHub in at least three mirrors
(`artism90/xiii-unrealscript`, archived read-only in December 2020; `Ch0wW/xiii_unrealscript`;
`VideogameSources/XIII`). `[reported 2026-09-02]` Provenance, per the first mirror's own README: it
was uploaded by "Sonrat" on 2005-03-17 to a fan site that no longer exists, and it is most likely **the
Xbox branch** — it carries Xbox Live classes and Xbox-specific hard-coded adjustments — with some
classes stripped to default properties or missing (`XIIIMenuMulti` accounting/online classes, the
`Mioche` character).

**Provenance caveat, stated plainly.** This is a leaked source tree with no licence; the rights holder
is Ubisoft. Under this project's rules it is **study material only** — read online, describe in our
own words, copy nothing, credit the mirrors. It is also the Xbox branch, so any PC-specific behaviour
must be confirmed against the shipped PC packages (UE Explorer still decompiles their bytecode —
`StripSource` removes the script *text*, not the code — it just loses the comments, which is exactly
what the leak restores).

Packages present: `Core, Editor, Engine, GUI, Gameplay, IpDrv, UWeb, XIDCine, XIDInterf, XIDMaps,
XIDPawn, XIDSpec, XIII, XIIIArmes, XIIIDeco, XIIIMP, XIIIPersos, XIIIPersosG, XIIIVehicule`. The names
are French: `XIIIArmes` = weapons (`fpsberretta`, `fpskalash`, `fpssniper`, … with `Fps.uc` and
`XIIIArmes.uc` as the base classes), `XIIIPersos` = characters, `XIIIVehicule` = vehicles. **`XIDPawn`
is AI and scripted-sequence pawns, not the player** (`BaseSoldier`, `IAController`, `SharkController`,
patrol/attack points) — the 2026-08-25 topic's guess that it owns pawn/weapon-aim logic was wrong;
the aim chain is in `Engine`.

## The aim chain, read from `Engine/Classes` — where VR pose would go

Both fire paths in `Engine/Classes/Weapon.uc` (about 1,850 lines) start the same way `[reported
2026-09-02, from source]`:

```
GetAxes(Instigator.GetViewRotation(), X, Y, Z);
Start       = GetFireStart(X, Y, Z);          // native: eye position + FireOffset in view axes
AdjustedAim = Instigator.AdjustAim(AmmoType, Start, 0);
```

for `ProjectileFire` and `TraceFire` alike. So the fire **origin** is the pawn's eye position plus a
weapon offset expressed in the **view** axes, and the fire **direction** is whatever
`Pawn.GetViewRotation()` returns — in UE2 that is the controller's `Rotation` for a player-controlled
pawn — passed through `AdjustAim`.

On `Engine/Classes/PlayerController.uc` (also about 1,850 lines):

| member | what it is | why it matters for M2 |
| --- | --- | --- |
| `event PlayerCalcView(out actor ViewActor, out vector CameraLocation, out rotator CameraRotation)` | **script event**, not native — first-person branch calls `native(497) CalcFirstPersonView(...)` (eye position + bob + shake), `bBehindView` calls `CalcBehindView` | this is the function M1 already hooks from the native side; the source shows the camera rotation is derived from the controller `Rotation` and that head **bob and shake** are folded in here |
| `rotator AdjustAim(Ammunition FiredAmmunition, vector projStart, int aimerror)` | script; **body commented out** in this branch | the aim-assist hook point — on the Xbox branch it is a no-op, so fire direction = view rotation exactly |
| `native(498) AdjustAimForDisplay(Ammunition, vector projStart)` | native | the *displayed* aim; XIII smooths the on-screen weapon separately |
| `vector OldAdjustAim`, `ViewAdjustAim`, `AdjustedAimForFiring`, `MemAim` | XIII-specific | "aim to use when firing projectiles (because display don't show anticipation)", "used to smooth weapon rot on screen" — the game already keeps a **firing aim distinct from the displayed aim** |
| `UpdateRotation(float DeltaTime, float maxPitch)` | script | input → controller `Rotation`, plus view shake |
| `SetFOV`, `FOV(F)` exec, `ToggleZoom/StartZoom/StopZoom/EndZoom`, `AdjustView` | script | the FOV/zoom state machine; `ScopeFOV`/`iAltZoomLevel` on the weapon |

**What this means for motion-controlled aim.** The engine already separates the two rotations M2
needs: the **view** (what `PlayerCalcView` emits — where M1 adds the HMD) and the **aim** (the
controller `Rotation` that `GetViewRotation()` hands to the weapon). Today they coincide because the
mouse drives both. For VR:

- **Head = HMD:** replace, rather than add to, `CameraRotation` in the existing `eventPlayerCalcView`
  hook (M1 adds the HMD delta to the mouse rotation; with a hand-driven aim the base should be a
  recentred HMD pose, not the aim).
- **Hand = controller:** write the motion controller's yaw/pitch into the PlayerController's
  `Rotation` each tick — the `APlayerController::Tick` hook the automation harness already owns is the
  natural place — so every `GetViewRotation()` the weapon makes returns the hand. No weapon class
  needs touching; `Fps.uc`'s subclasses inherit the chain.
- **Origin:** `GetFireStart` is native and anchored at the *eye*; a hand-held origin means either
  accepting eye-origin rays along the hand direction (fine for a first pass) or hooking the native.
- **Disable the smoothing** the game applies to the displayed aim (`ViewAdjustAim`) and the shake/bob
  in `CalcFirstPersonView` — comfort, and the displayed weapon should follow the hand, not lag it.

## Concrete next steps

1. Read `Engine/Classes/Pawn.uc` (`GetViewRotation`, `EyePosition`, bob) and `XIIIArmes/Classes/Fps.uc`
   online to confirm the chain is not overridden game-side — one fetch each.
2. Decompile the **PC** `Engine.u`/`XIIIArmes.u` with UE Explorer only where the Xbox branch might
   differ (input, zoom), not for the whole tree.
3. When M2 starts: prototype hand-aim as "controller pose → `Rotation` in the Tick hook", measure the
   fire ray against the hand direction with the existing telemetry.

## Sources

- https://github.com/artism90/xiii-unrealscript — mirror with the provenance README (archived 2020-12-11)
- https://github.com/Ch0wW/xiii_unrealscript · https://github.com/VideogameSources/XIII — further mirrors
- https://beyondunrealwiki.github.io/pages/unrealscript-source.html — notes that XIII's shipped packages had their script text stripped
- https://www.oldunreal.com/phpBB3/viewtopic.php?t=10021 — OldUnreal's reverse-engineered UT2004 native headers (Object/Interaction/Actor only; no render interface), for the record
