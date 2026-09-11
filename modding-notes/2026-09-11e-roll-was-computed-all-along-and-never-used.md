# 2026-09-11e — roll was being computed all along and never used

**`/pd`, dev PC (`DESKTOP-V8GTSIR`), auto-picked. The game was NOT launched. Nothing here has been run.**

XIII's last `[PD]` row: *"yaw right, pitch a little off, roll tilts the world, and it is shaky"*.
Parts (a) and (b) are done; (c) is analysed and left, with the reason.

---

## 1. ⭐ (a) Roll — both halves of the fix already existed

`camera_hook.cpp` applied yaw and pitch and stopped. But:

- **`FRotator` already has a `Roll` field** (`struct FRotator { int32_t Pitch; int32_t Yaw; int32_t Roll; }`).
- **`QuaternionToEuler` already returns `roll`** (`struct EulerRadians { float yaw; float pitch; float roll; }`).

**The value was computed on every single frame and thrown away** `[inferred-static 2026-09-11]`. That
is the second time today the same shape has turned up on this project — a fact already in hand,
discarded one line before it was needed.

**Why it tilts the world.** Nothing else counter-rotates the image: XIII's picture is submitted as
flat stereo halves, so there is no compositor applying head roll for us. Tilt your head and the
displayed image tilts with it — which reads as *the world* tilting, because your head is the only
thing that moved.

**The sign is a genuine unknown, so it is a signed ini key, not a constant.** Yaw already has to be
negated because OpenVR winds opposite to Unreal, and nothing establishes that roll follows the same
convention. `[VR] CameraRollScale` therefore takes **1.0 (default), -1.0, or 0**, clamped to ±4, and
the applied value is logged once at hook install so the log always states what is in force. **This is
the same remedy, for the same reason, as `re-village-scope-vr`'s `roll_k`** — recorded there as
*"SIGNED, because the sweep did not pin the sign either"*.

⚠️ **Default 1.0, not 0.** House style elsewhere is off-by-default so the picture is unchanged until
someone opts in — but here the current behaviour is *known broken*, so shipping it off would mean the
next launch still tilts unless somebody remembers the key. **`CameraRollScale=0` restores the old
behaviour exactly** for anyone who wants it.

## 2. ✅ Verified numerically, against the shipped code, with a proved-failing control

Roll had exactly **one** test — that identity gives zero — which cannot catch a wrong axis, a wrong
sign, or roll leaking into pitch. All three now matter. **Four cases added**, driving the shipped
`pose_math`, not a transcription:

1. a 30° roll quaternion returns 30° of roll;
2. **a pure roll does not leak into yaw or pitch**, swept −60°…+60° — the nastiest failure, because
   tilting your head would also look up or down, which is far harder to diagnose in a headset than a
   plain wrong sign;
3. the conversion the hook actually performs (`scale · roll / 2π · 65536`) against independently
   written expectations — a quarter turn is a quarter of 65536, and so on;
4. `scale 0` contributes exactly nothing and `-1` is the exact mirror of `+1`, **with a non-vacuity
   check** so the case cannot pass trivially at zero roll.

**14 cases, 50 assertions, all passing** `[verified-numerically 2026-09-11]`.

**🚨 And the suite was proved able to fail.** A test that cannot produce a negative is not evidence, so
the roll sign in the shipped `pose_math.cpp` was deliberately flipped: **2 cases and 5 assertions
failed**, exactly the ones that should. Reverted, `git status` clean on that file, suite green again
`[verified-numerically 2026-09-11]`.

## 3. ✅ (b) Pitch — the comparison the row asked for is now in the log

The row's instruction was *"compare against the engine's view pitch in the log"*. The hook now
captures the engine's **own** pitch and roll **before** touching them and logs, once every ~600 calls:

```
view: enginePitch=… ourDPitch=… -> … | engineRoll=… ourDRoll=… (scale 1.00)
```

That is the whole of what (b) needs: if our delta is right but the result is wrong, the game is
clamping or offsetting afterwards, and the log shows it in one line.

## 4. ⚠️ (c) Shake — analysed, deliberately not attempted

The row names two contributors and they need different work:

- **No frame-time prediction.** The pose is read on the OpenXR worker thread and consumed in
  `PlayerCalcView` with no `xrLocateViews` at the predicted display time. Fixing that properly means
  restructuring when the pose is sampled relative to the frame — **not a two-line change**, and one
  that interacts with the submission path.
- **Frame pacing.** The stereo log already shows 24–72 fps swings, and no amount of prediction
  smooths a frame rate that is itself lurching.

**Attempting prediction before knowing how much of the shake is pacing would be guessing at the
split.** The roll fix lands first because it is unambiguous; whatever shake remains after a run with
roll correct is the honest measurement of what prediction would have to fix.

**Build:** exit 0; `CameraRollScale` strings confirmed present in the binary; exports re-checked
against the stock device, **none missing** `[verified-numerically 2026-09-11]`. Deployed (previous
kept as `.bak-2026-09-11-pre-roll`), re-stamped 4 files ALL MATCH, dated artefact preserved.

## 5. What one launch now answers

With `[VR] CameraLiveHmd=1`:

- **Tilt your head. World stays level ⇒ (a) is fixed** at the default sign.
- **World tilts twice as hard ⇒ the sign is inverted**: set `[VR] CameraRollScale=-1.0`. No rebuild.
- **No change at all ⇒ `PlayerCalcView` ignores the `Roll` field**, which is the outcome the row
  explicitly allowed for — and then roll has to be applied in the view matrix instead. The log line
  distinguishes this from a sign problem, because it shows a non-zero `ourDRoll` going in.
- **Read `enginePitch` vs `ourDPitch`** on the same run to settle (b) at no extra cost.

---

## What is NOT established

- **Nothing here has been run.** No frame has been rendered with roll applied, on either machine.
- **The sign.** 1.0 is a guess, deliberately exposed as a key rather than baked in.
- **That `PlayerCalcView` honours `Roll` at all.** The hook writes the field; whether UE2 consumes it
  for the view is exactly what the launch decides.
- **How much of the shake is prediction versus pacing.** Both are real; the split is not measured, and
  was not guessed at.
- **That the pitch offset is ours rather than the engine's.** The log will say; today it only asks.
