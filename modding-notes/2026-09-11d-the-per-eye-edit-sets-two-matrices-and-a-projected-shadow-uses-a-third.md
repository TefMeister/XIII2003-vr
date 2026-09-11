# 2026-09-11d — the per-eye edit sets two matrices, and a projected shadow would use a third

**`/pd`, dev PC (`DESKTOP-V8GTSIR`), auto-picked. The game was NOT launched. Nothing here has been run.**

The ⭐ shadows row asked to *"classify which bucket the shadow draws land in and whether the projector
matrix is per-eye"*. The second half is answerable from our own source without any launch at all, and
the answer is **no** — with a mechanism that fits the reported symptom exactly.

---

## 1. ⭐ What the per-eye edit actually sets — and what it leaves alone

The fixed-function stereo path, verbatim, is two lines `[inferred-static 2026-09-11]`:

```
s_realSetTransform(dev, D3DTS_VIEW_,       s_eyeView[eye]);
s_realSetTransform(dev, D3DTS_PROJECTION_, s_eyeProj[eye]);
```

**That is the whole of it.** And `Hook_SetTransform` tracks only `D3DTS_WORLD`, `D3DTS_VIEW` and
`D3DTS_PROJECTION` — **texture-stage matrices (`D3DTS_TEXTURE0..7`) were passed straight through,
untracked and unadjusted.**

**🚨 Why that matters for shadows specifically.** The standard D3D8 fixed-function way to put a
character's shadow on the floor is a **projected texture**: the light's view-projection is baked into
a **texture matrix**, and texture coordinates are generated from the camera-space position of the
receiving surface. If XIII does that, then per eye:

- the **receiver** moves, because we offset `VIEW` and `PROJECTION`;
- the **shadow's projection does not**, because it lives in a matrix we never touch.

**⇒ The shadow does not travel with the surface it is painted on, so the two fuse at different
depths.** That is precisely *"character shadows look spatial and 3D somehow"* — a shadow that reads
as a separate object floating near the floor rather than as a mark on it
`[hypothesis 2026-09-11 — the mechanism is read from our own source and is solid; that XIII uses a
projected texture is the premise, and it is exactly what the counter below tests]`.

⚠️ **A subtlety worth writing down, because it changes what "unadjusted" means.** If the game uses
`D3DTSS_TCI_CAMERASPACEPOSITION`, the generated texcoords *do* change when we change `VIEW` — so the
shadow would shift per eye, but by the **wrong amount**, because the texture matrix mapping camera
space into light space was built for the original view. **Either way the result is a mismatch**, so
the hypothesis survives both readings; only the *size* of the error differs.

## 2. ✅ What was built — one counter that can kill the hypothesis outright

Texture-matrix sets are now **observed but still deliberately not adjusted**:

- `texmat=` joins the once-per-second heartbeat — how many texture-matrix sets per frame.
- **A one-time line names which stages carry one**, the first time any does:
  `TEXTURE MATRICES IN USE on stage(s): 1 -- these are NOT adjusted per eye; suspect for shadows at the wrong depth`

**`[compile-verified 2026-09-11]`** — exit 0, both new strings confirmed present in the binary by
string search, exports re-checked against the stock device (**32 names each side, none missing**)
`[verified-numerically 2026-09-11]`. Deployed (previous kept as `.bak-2026-09-11-pre-texmat`),
re-stamped 4 files ALL MATCH, dated artefact preserved in `staging`.

**Nothing about stereo behaviour changed.** The row asked for classification, and changing the
rendering before knowing whether the premise holds would be exactly the speculative fix this estate
keeps learning not to make.

## 3. What one launch now answers — and the zero case is the valuable one

Read the heartbeat and the one-time line on **any** run of the stereo build:

- **`texmat` stays 0 for a whole session, no stage line** → **the projected-shadow hypothesis is dead
  outright.** XIII does not use texture matrices at all, the shadow cannot be a projected texture, and
  it must be geometry (a blob mesh) — which our stereo *does* handle per eye, so the "3D" look would
  then have a different cause entirely and this line of work closes. **A clean negative, obtainable
  for free.**
- **`texmat` > 0 and a stage line appears** → the premise holds. The named stage is where the shadow's
  projection lives, and **adjusting that matrix per eye becomes the fix** — a third matrix alongside
  the two the eye path already writes.
- **`texmat` > 0 but shadows still look wrong after that fix** → the shadow is not what those matrices
  are for (they are used for plenty of other effects), and the bucket question from the row's other
  half is still open.

⭐ **Note the shape: the cheapest outcome is the one that closes the row.** That is what makes this
worth doing before any of the more expensive shadow work.

---

## What is NOT established

- **Nothing here has been run.** No frame has been rendered with the counter, on either machine.
- **That XIII draws shadows as a projected texture at all.** That is the premise, and it is untested
  — the counter exists precisely because it is cheap to test and would otherwise be assumed.
- **That texture matrices, if present, belong to the shadow.** They are used for scrolling effects,
  environment mapping, decals and more. A non-zero `texmat` narrows the field; it does not identify
  the shadow.
- **Which bucket the shadow draws land in** — the row's *other* half. That still needs the per-class
  counters read during a scene with a visible character shadow, which needs a launch.
- **That adjusting the texture matrix per eye is even well-defined here** — it depends on what the
  matrix maps from. Worth deciding after the stage is known, not before.
