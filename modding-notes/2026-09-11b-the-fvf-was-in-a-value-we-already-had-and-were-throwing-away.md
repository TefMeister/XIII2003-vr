# 2026-09-11b — the HUD's decisive fact was in a value we already had and were discarding

**`/pd`, dev PC (`DESKTOP-V8GTSIR`). The game was NOT launched. Nothing here has been run.**

The ⭐⭐ HUD row's named cheapest first step was *"read our own tile path — does it use an ortho
matrix, or `D3DFVF_XYZRHW` pre-transformed vertices?"*, because the two need completely different
fixes and matrix manipulation cannot touch the second. Reading the renderer answered a better
question than the one asked.

---

## 1. ⭐ We could not tell, and the reason is that we were throwing the answer away

**How draws are classified today** (`stereo_hook.cpp`):

- `s_projPersp = (m[11] == 1.0f && m[15] == 0.0f)` — i.e. `_34 == 1`, `_44 == 0`. That is the
  standard perspective test, and it is correct as far as it goes `[inferred-static 2026-09-11]`.
- `StereoDraw()` gates on it: `if (!s_haveP || !s_projPersp) { s_cMonoOrtho++; return draw(); }`.
  Anything with a perspective projection falls through and **gets stereo'd**.

**🚨 The hole: a `D3DFVF_XYZRHW` draw bypasses the transform pipeline entirely, so the projection
matrix in force says nothing whatever about it — and the projection matrix is the only thing the
classifier looks at.** If the game leaves a perspective projection set while drawing pre-transformed
2D — and nothing obliges it to set an ortho one for vertices that ignore the matrix — those draws
sail past the `mono-ortho` gate and are handed a per-eye matrix edit they cannot meaningfully
consume. **That is exactly the shape of the reported defect** (*"hud elements don't line up … it
slides around my view"*) `[hypothesis 2026-09-11 — the mechanism is read from our source; that XIII
actually does this is what the counters below will settle]`.

**⭐ And the fact needed to see it was already in our hands.** `Hook_SetVertexShader(void* self,
DWORD handle)` asks only *"is this handle one of the programmable shaders we saw created?"* and
discards the rest. **In D3D8, a handle that is not a created shader IS an FVF code** — so the
position type, `D3DFVF_XYZRHW` included, was arriving in a parameter we already intercepted, on every
single draw, and being dropped on the floor `[inferred-static 2026-09-11]`.

⚠️ **The transferable bit:** the row asked for a *static read of the game's behaviour*. The answer was
a **static read of our own instrument** — we had been unable to distinguish two cases because our
classifier never looked at the field that distinguishes them. *Before concluding you need a
measurement from the game, check what your own code is already discarding.*

## 2. ✅ What was built — measurement, deliberately not a fix

`s_curVsXyzrhw` is now shadowed alongside `s_curVsProgrammable`:

```
s_curVsXyzrhw = (!prog && (handle & 0x00E) == 0x004);   // POSITION_MASK, XYZRHW
```

and two counters split pre-transformed draws by where the classifier sends them, reported in the
existing once-per-second heartbeat as **`rhw-mono=` / `rhw-stereo=`**:

- **`rhw-mono`** — pre-transformed draws that already go mono (the ortho gate caught them anyway).
- **`rhw-stereo`** — pre-transformed draws that got past **every** mono gate and are about to be
  stereo'd. **Any non-zero value here is the smoking gun.**

**`[compile-verified 2026-09-11]`** — builds clean, exit 0, and the new format string is confirmed
present in the binary by string search (the DLL came out the same 206,336 bytes as the previous
build, which is padding, not a failed compile — worth checking rather than assuming). Exports
re-checked against the stock device: **32 names each side, 32 overlapping, none missing**
`[verified-numerically 2026-09-11]`.

Deployed as `system\D3DDrv-m2-stereo.dll` (previous kept as `.bak-2026-09-11-pre-rhw`), re-stamped,
4 files ALL MATCH. Dated artefact preserved in `staging` as
`D3DDrv-m2-stereo-2026-09-11b-rhw.dll`.

### ⚠️ Why the obvious fix was NOT applied

Forcing every XYZRHW draw to mono is the candidate fix and it is one line away. It was deliberately
left out, because **which way it should go depends on a fact nobody has**: the stereo path sets
per-eye **viewports** as well as matrices, so drawing pre-transformed 2D twice into two half
viewports is not obviously wrong — it might be exactly how a HUD should be duplicated per eye. The
counters decide whether the sliding elements are even in this bucket before anything changes
behaviour. **Measuring first is the whole point of this being the cheapest step.**

## 3. What one launch now answers

Run the stereo build flat and read the heartbeat line:

- **`rhw-stereo` > 0** → pre-transformed 2D *is* being stereo'd. That is the HUD defect's mechanism,
  and forcing that bucket mono is the next change.
- **`rhw-stereo` = 0 and `rhw-mono` > 0** → the 2D path is pre-transformed but already handled
  correctly, so the sliding HUD has a **different** cause and this line of work is closed.
- **both 0** → XIII's 2D does not use pre-transformed vertices at all; it is the ortho route, the
  `mono-ortho=26/frame` already seen is the whole of it, and the fix is the *tunable HUD depth*
  described in dossier §11d rather than anything to do with FVF.

**All three outcomes are useful, and they are mutually exclusive** — which is what makes this worth a
launch rather than an argument.

---

## What is NOT established

- **Nothing here has been run.** No frame has been rendered with these counters, on either machine.
- **That XIII draws its HUD with pre-transformed vertices at all.** That is the hypothesis the
  counters exist to test; the *mechanism* by which it would break is read from our own source and is
  solid, but the *premise* is not yet evidence.
- **That forcing XYZRHW to mono would fix the sliding**, even if `rhw-stereo` turns out non-zero —
  see §2 on the per-eye viewport question.
- **That this is the only cause.** Dossier §11d records that the objective marker is world-anchored
  and in a different bucket again; nothing here touches that.
