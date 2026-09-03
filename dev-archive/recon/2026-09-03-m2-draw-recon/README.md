# M2 draw recon — the run that decided the stereo route (2026-09-03)

`xiii_transform.log` — the raw output of one flat gameplay session on the dev PC with the
`TransformRecon` build active as the render device. **26,595 frames, 343 one-second intervals,
2,076,870 `SetTransform` calls, 3,254,942 draws**, ~5.5 minutes of menu, indoor, outdoor and a
cutscene.

Produced by `D3DDrv-m2-recon` (186,880 bytes, built 2026-09-02, kept as
`staging/XIII2003-vr/D3DDrv-m2-recon-2026-09-02b.dll`) with `[VR] TransformRecon=1` and
`TransformReconDump=12`. Written to `%TEMP%\xiii_capture\xiii_transform.log`, which is outside
every repository — hence this copy.

**It is committed because it is the sole evidence for a design decision.** Regenerating it costs
a full flat session plus a DLL swap on a machine whose live build (0.2.9) has no source. It is data
this project generated, not game content, so it is within the never-upload-game-files rule.

> ⚠️ `dev-archive/.gitignore` carries a blanket `*.log`. This file is **not** currently caught by it,
> but that is worth re-checking rather than assuming — on 2026-09-03 the sibling project
> `the-evil-within-vr` was found to have "rescued" a table into `recon/` two days earlier that git
> had silently refused, because of exactly such a rule. **Verify with
> `git ls-files --error-unmatch <path>`, never by seeing the file in the folder.**

## What it decided

**Route (b) alone — draw every batch twice at D3D8 — is ruled out.** The programmable vertex-shader
path is live and constant: `programmable-VS > 0` in **338 of 343 intervals**, **8.72 %** of all
draws frame-weighted, peaking at **51.4 %** of a second's draws. M2 needs route (b) *plus* a
vertex-shader-constants path. Live constant registers, in 342 of 343 intervals:
`c10 x1  c0 x8  c0 x5` — where `c0 x5` confirms the static prediction from the `+0x4205` call site.

Full analysis, caveats and what is *not* established:
[`modding-notes/2026-09-03-m2-route-decided-the-programmable-path-is-real.md`](../../../modding-notes/2026-09-03-m2-route-decided-the-programmable-path-is-real.md).
Distilled durable version: `engine-research/ENGINE-DOSSIER.md` §7a.

## Reading it

- `t=<n>s ... | per frame: ... | equals-cache skips: ...` — one line per interval, the whole session.
- `draws/frame: fixed-function=... programmable-VS=... | ... | VSconst regs: ...` — the line under it.
- `=> ...` — the recon's own verdict for that interval (339 say route (b) is insufficient; 4 say it
  would suffice).
- `world|view|proj #N frame=... ` blocks — the first 12 matrices of each type.
  **⚠️ All 12 landed at `frame=574`, in the main menu, before gameplay started.** Every dumped matrix
  is a menu identity or the menu camera; **no gameplay matrix is in this file.** The per-interval
  counters are unaffected — they ran for the whole session and are what decided the route.
