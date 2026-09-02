# The M2 aim lever is a read: XIII's UnrealScript source is public, and the fire direction is `Pawn.GetViewRotation()`

Filed by: `/gr`, 2026-09-02
Topic: `external-research/topics/2026-09-02-xiiis-unrealscript-source-is-public-so-the-m2-aim-lever-is-a-read-not-a-decompile.md`
Dossier sections: §2 ("decompilable to near-source … a lever not yet pulled"), §12 (6DOF / motion-controlled aim)

`[reported 2026-09-02, read online from GitHub mirrors of a 2005 leak of the Xbox branch — study only, nothing copied, rights holder Ubisoft]`

- **Source, not bytecode.** `artism90/xiii-unrealscript` (archived), `Ch0wW/xiii_unrealscript`, `VideogameSources/XIII` carry the full package tree (`Engine`, `XIIIArmes` = weapons, `XIIIPersos` = characters, `XIDPawn` = **AI/scripted pawns, not the player**). PC packages are `StripSource`d (text gone, bytecode intact), so UE Explorer still works for PC-vs-Xbox diffs; the leak restores the comments.
- **Fire chain (`Engine/Classes/Weapon.uc`, both `ProjectileFire` and `TraceFire`):** `GetAxes(Instigator.GetViewRotation(), X,Y,Z); Start = GetFireStart(X,Y,Z) /*native: eye + FireOffset in view axes*/; AdjustedAim = Instigator.AdjustAim(AmmoType, Start, 0)`. Direction = the controller's `Rotation`; origin = the eye.
- **`PlayerController.uc`:** `PlayerCalcView` is a **script event** (first-person → `native(497) CalcFirstPersonView`, which folds in bob and shake); `AdjustAim(...)` has its body commented out on this branch; XIII keeps a firing aim distinct from the displayed aim (`AdjustedAimForFiring`, `ViewAdjustAim`, `OldAdjustAim`).

## Suggested dossier changes

- §2: replace "decompilable … lever not yet pulled" with "source is public (Xbox branch, 2005 leak); read it, decompile PC packages only to diff".
- §12: the aim design is now concrete — **head = HMD** by *replacing* `CameraRotation` in the existing `eventPlayerCalcView` hook; **hand = controller** by writing the motion-controller yaw/pitch into the PlayerController `Rotation` from the `APlayerController::Tick` hook the harness already has, so every `GetViewRotation()` the weapon makes returns the hand. Origin stays at the eye unless `GetFireStart` (native) is hooked. Disable `ViewAdjustAim` smoothing and bob/shake for comfort.
