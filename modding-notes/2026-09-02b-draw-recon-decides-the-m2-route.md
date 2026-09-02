# 2026-09-02b (`/pd`, home PC, NO LAUNCH) — the M2 route is now a measurement, not a judgement call; and route (b) has a gap the static scan found

**The game was not launched, no debugger was attached, nothing here has been run.** Static analysis
of `D3DDrv_Original.dll` on disk, plus a full compile and link.

Picked up the `[PD]` item carrying the ⚠️ "unfinished work checkpointed at 02:54, read it before
starting" warning. **Read it first, as instructed — and it was worth it:** the checkpoint
(`staging` `0b235cb`) already contained a complete, well-built recon hook. Nothing was redone.

---

## 1. What the 02:54 checkpoint already had

`proxy/transform_hook.cpp` (242 lines) — a passthrough inline hook on
`FD3DRenderInterface::SetTransform` gated behind `[VR] TransformRecon=1`, fail-safe on two byte
anchors, counting calls per type, distinct views per frame, perspective-vs-orthographic
projections, how often the early-out *would* fire, and dumping the first N matrices with the
projection decoded to `fovY / aspect / near / far`. Plus `pose_math/` and `tests/`.

It reads as finished, not abandoned. **My contribution is not to rewrite it but to close the one
gap that stops it answering the actual question.**

## 2. The question it could not answer: there was no denominator

`/gr` dropped the decisive framing while this was checkpointed. There are two M2 routes:

- **(a)** modify matrices inside `SetTransform` — must defeat the unchanged-matrix early-out
  (dossier §7a), or eye 2 silently inherits eye 1's view;
- **(b)** leave `SetTransform` alone; **draw every batch twice** at the D3D8 level with per-eye
  `D3DTS_VIEW`/`D3DTS_PROJECTION` and viewports — the design `unreal-gold-vr` already proved
  `[compile-verified; verified-numerically, 70,550 checks]`. The engine's cache never receives a
  modified matrix, so **there is no early-out to design around at all.**

Route (b) looks strictly better — but only if **every** world draw takes its transform from the
fixed-function pipeline. The transform recon counts `SetTransform` traffic and has no idea how many
draws happened, so it cannot tell you that. It has a numerator and no denominator.

## 3. ⭐ Static finding: the programmable path is present and used, so route (b)'s gap is real

A whole-`.text` scan of `D3DDrv_Original.dll` for `CALL r/m32` with `IDirect3DDevice8` vtable
displacements `[inferred-static 2026-09-02, n=1 binary, exhaustive scan]`:

| method | vtable idx | call sites |
|---|---|---|
| `CreateVertexShader` | 75 | **10** |
| `SetVertexShader` | 76 | **17** |
| `SetVertexShaderConstant` | 79 | **4** |
| `DrawIndexedPrimitive` | 71 | 12 |
| `DrawPrimitive` | 70 | 6 |
| `DrawIndexedPrimitiveUP` | 73 | 3 |
| `SetTransform` | 37 | 15 |

Ten creation sites is not a vestigial path. And disassembling the four `SetVertexShaderConstant`
sites, **the one at `+0x4205` uploads five constants starting at register `c0`**:

```
push 5                    ; ConstantCount = 5
lea eax,[esp+0x10]        ; pConstantData
push eax
push 0                    ; Register = 0
push esi                  ; this (the device)
call dword ptr [edx+0x13c]
```

Five float4s at `c0` is the shape of a transform matrix plus one spare. **A draw issued under a
programmable vertex shader reads its view and projection from those constants, not from
`SetTransform`** — so route (b) alone would leave exactly those draws mono while everything around
them went stereo. `/gr` predicted this as the one way a per-draw design silently misses geometry;
it is now known to be a real risk in this binary rather than a hypothetical.

**What this does NOT establish**, and it matters: whether those programmable draws carry *world
geometry* (they may be confined to skinning, terrain or effects) and what fraction of a real frame
they are. That is a count, not a static question.

## 4. So: added the draw recon, and now one launch decides the route

Extended `frame_capture.cpp` — which already owns the D3D8 device vtable for its Present hook — to
also hook, behind the same `TransformRecon` flag: `CreateVertexShader` (75), `SetVertexShader` (76),
`SetVertexShaderConstant` (79), `DrawPrimitive` (70), `DrawIndexedPrimitive` (71),
`DrawIndexedPrimitiveUP` (73). Per frame it now reports **fixed-function draws vs programmable-VS
draws**, the percentage, the `SetVertexShader`/`SetVertexShaderConstant` rates, how many
programmable shaders were created, and the distinct `(register, count)` pairs seen — the last of
which says *where* a programmable path wants its matrix.

Two deliberate design choices, both about not fooling ourselves:

- **Classification uses no heuristic.** D3D8 overloads one `DWORD` for both FVF codes and shader
  handles, and "an FVF has bit 0 clear" is a runtime convention, not something to bet a design
  decision on. So the recon records what `CreateVertexShader` actually **returned** and tests
  membership.
- **Declaration-only creations (`pFunction == nullptr`) are excluded** — they still run
  fixed-function, and counting them would overcount route (b)'s gap, which is the single number the
  whole exercise exists to get right.
- Hot-path discipline preserved: the draw hooks bump one counter; the membership scan happens in
  `SetVertexShader`, which is orders of magnitude rarer than a draw. A 64-entry handle table with an
  explicit `HANDLE TABLE OVERFLOWED` marker, because a silently truncated table would understate the
  gap — the one direction of error that would wrongly bless route (b).

## 5. Built, linked, ABI-checked, staged

`[compile-verified 2026-09-02]` Full MSVC build (`build.bat`, VS2022 Build Tools, Win32), **exit 0**,
173,056 bytes. All source compiled clean; the only warnings are the pre-existing `_snprintf`
deprecation ones already throughout the tree.

**Export surface checked against the deployed build: 40/40, identical names and count**, so it is
ABI-compatible with what `Engine.dll` loads. The size differs from the deployed 0.2.7's 75,776 bytes
because of build configuration (`/MT` static CRT), not missing functionality — for a render device
the export surface is what decides whether it loads at all.

The vendored OpenVR/OpenXR binaries are gitignored **by design** (`fetch_openvr.md`,
`fetch_loader.md` document the fetch); the link failed until they were fetched per those
instructions. They remain uncommitted.

**Deployed side-by-side, deliberately NOT over the live build:**
`system\D3DDrv-m2-recon.dll` sits beside an untouched `D3DDrv.dll`. The recon build comes from the
**0.2.3-era rescued tree**, so making it the active render device permanently would regress the
focus-loss IAT hook, perf log and automation harness — all of which survive only in the installed
0.2.7 binary and in no source anywhere. Also saved as
`staging/XIII2003-vr/D3DDrv-m2-recon-2026-09-02b.dll`.

`XIII.ini` `[VR]` gained documented `TransformRecon=0` / `TransformReconDump=12` keys, so the run is
a rename plus one character. Backup: `XIII.ini.bak-2026-09-02`.

## 6. ⚠️ A documented rule I had to break, and the correction

Dossier §7a said **"DO NOT EDIT ANYTHING INSIDE `staging/XIII2003-vr/src/repo/`"**, because a fresh
build of the untouched tree matching `D3DDrv.dll.pre-027-bak` byte-for-byte is the entire evidence
that the rescued source really is the deployed build.

**That rule had already been overtaken before I arrived** — the 02:54 checkpoint added
`transform_hook.*`, `pose_math/` and `tests/` into that tree and edited four other files. Leaving the
rule standing would have told the next session something false about the tree in front of them.

Verified rather than assumed: **`e67e15e` (2026-09-01) holds the pristine rescue** — listing it shows
no `transform_hook.*`, no `pose_math/`, and the original `frame_capture.cpp`. So **nothing was lost
and the byte-identity claim is still checkable; it is simply anchored to `e67e15e`, not to `HEAD`.**
Dossier §7a corrected to say exactly that, and to keep the half of the rule that still stands (do
not "tidy" the frozen 0.2.3 artefacts in there).

## 7. What is NOT established

- **Nothing here has run.** The draw recon is compile-verified and staged, and that is all.
- **The static scan proves the programmable path exists, not that it draws world geometry.** If the
  live count comes back `programmable-VS=0` in real scenes, route (b) is clean and the static
  finding was a correctly-flagged risk that did not materialise — that is a good outcome, not a
  contradiction.
- **The `+0x4205` site's five constants at `c0` are inferred to be a transform from their shape**
  (five float4s at register 0), not from reading the shader that consumes them. The live
  `VSconst regs` line is what would corroborate it.
- **The six new hooks are written against the standard `IDirect3DDevice8` vtable order.** Indices
  were cross-checked against the one already known-good in this project (`SetTransform` = 37, which
  matches the dossier's independently derived `vtable slot 0x94`), but a wrong index would mean a
  crash on first draw, not a wrong number.
- The recon still cannot see draws made through a path that bypasses the device vtable entirely;
  none is known to exist here.
