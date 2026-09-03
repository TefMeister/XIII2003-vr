# The `OpenXrProjectionTestOffsetM` experiment is the weaker half — here is the stronger one

*Dropped by `/sr`, 2026-09-03. Create-only; fold into `ENGINE-DOSSIER.md` §12 and delete.*

**Supersedes nothing** — the dossier's §12 claim is correct as written. This narrows one thing it
records as "untested per runtime" into something with a public record behind it, and it changes what
the already-built test should look for.

## What the dossier currently says, and what is new

§12 records: the projection-layer submission path is built `[compile-verified 2026-09-02]`,
`[VR] OpenXrProjectionTestOffsetM=0.15` submits the two views with deliberately opposite lateral
offsets, and *"image visibly splits ⇒ the runtime honours independent per-view poses; both eyes
identical ⇒ it ignores them"*. The risk is tagged as moved from "impossible" to "untested per
runtime" `[reported]`.

**New this sweep, `[reported 2026-09-03]`, read online:** it is not merely untested. There are two
public first-hand reports, and they **contradict each other about which runtime is at fault** —
which is itself the finding.

| when | who | what they report |
|---|---|---|
| 2020-10-29 | **LukeRoss00** (the OpenVR #1253 filer), on Valve's [SteamVR discussion board](https://steamcommunity.com/app/250820/discussions/8/3001046778344834329/) | Spec-correct per-view poses from `xrLocateViews`, submitted through `xrEndFrame`, gave a **wrong stereo baseline and a vertical offset between the eyes** on a Valve Index (SteamVR 1.15.4, OpenXR runtime 0.1.0). Workaround: **head pose for both views**, plus swap the views' `fov.angleDown`. Oculus and Microsoft runtimes recorded as correct. No reply. |
| 2023-09 | **SirKandela** (Chaos LTD) with **Rylie Pavlik**, on the [Khronos forums](https://community.khronos.org/t/oculus-runtime-ignores-projection-layer-views-pose/110078) | The **Oculus desktop runtime appeared to ignore** the submitted projection-view pose while **SteamVR respected it** — the exact inverse. Pavlik's counterpoint: a runtime truly ignoring the pose could not reproject correctly at all, so "ignored" may misdescribe the observation. |

⇒ **Per-view pose handling is runtime- and version-specific, and it has changed over time.** Record
the runtime **name and version** with whatever the headset run produces; without those the result is
not transferable, not even to the same machine after an update.

## ⚠️ The practical consequence: the offset test can produce an ambiguous null

Khronos's own reference text for `XrCompositionLayerProjectionView` says `pose` and `fov` *"should
almost always derive from"* the `XrView` values returned by `xrLocateViews`. A deliberate 0.15 m
synthetic offset is therefore exactly the off-the-beaten-path submission a runtime is **least** likely
to have been tuned for. So "both eyes identical" would be ambiguous between:

1. the runtime collapses per-view poses (the conclusion the test wants to draw), and
2. the runtime declined an implausible pose and fell back to the head pose.

That is the [silent no-op](../ENGINE-DOSSIER.md) shape this project already knows to distrust: a null
that cannot distinguish "the feature is absent" from "the input was rejected".

**Don't remove the offset test — it is cheap and a clean split is genuinely informative. Add the
stronger one beside it:** submit the **legitimate located per-view poses** and look for LukeRoss's
failure signature — a visibly wrong **stereo baseline** together with a **vertical misalignment
between the eyes**. Vertical disparity is the valuable half: nothing in a correct stereo pair produces
it, so seeing it is a *positive identification* of the defect rather than the absence of an expected
effect.

Given the note in §12 that both views currently reference the same swapchain image (mono content
through two per-eye frusta), a legitimate-pose run is also exactly what that build already produces
with `OpenXrProjectionTestOffsetM=0`, so this may cost no code at all — just a second reading of the
same run with the right thing being looked for.

⚠️ **And if the defect shows up, the workaround is documented:** head pose for both views plus the
`angleDown` swap. Worth knowing before concluding the M2 submission design is blocked.

## One run answers it for two projects

`far-cry-2-vr` is blocked on the identical question for its AER submission — §12 already says so, and
a matching pointer went into that repo's inbox this sweep. Whichever project reaches a headset first
should publish the runtime name and version with the result.

## Confidence

`[reported 2026-09-03]`. Two first-hand developer forum accounts, on runtime versions years old,
neither reproduced here. Risk moves from *"untested per runtime"* to *"known to vary between
runtimes"*. It does not say what your runtime does today.

Generalised form now in the cross-engine library: `docs/techniques/README.md` → "OpenXR carries a
pose per view where OpenVR collapses to one" → "⚠️ Expressible is not honoured".
