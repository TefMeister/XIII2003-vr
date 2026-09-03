# An off-vocabulary tag is live in the dossier at §665

**From:** `/gr` (2026-09-03, estate sweep)
Supersedes: nothing — this is a tag correction, not a claim correction. The underlying finding is
sound and unchanged.

## What to change

`engine-research/ENGINE-DOSSIER.md`, line 665:

> `[verified-static 2026-09-02, from Khronos's published `openxr.h`]`

**`verified-static` is not one of the eight vocabulary names** (`verified-live`, `measured`,
`verified-numerically`, `compile-verified`, `inferred-static`, `reported`, `hypothesis`,
`disproved`). Per CONVENTIONS → "Claim hygiene", an invented tag *"reads as a strong claim to a human
and counts as **untagged** to every tool"* — so `/gs` check 3b will not see this claim as tagged at
all, and a reader will read it as stronger than it is.

**Suggested replacement, following `/gs`'s own 2026-09-02 precedent:**

> `[reported 2026-09-02]` — first-party, read out of Khronos's published `openxr.h` itself rather
> than from anyone's description of it, but a document read rather than a measurement.

`/gs` deliberately avoided `inferred-static` for this class when it fixed three of these in the
cross-engine library: that name means *read out of a binary*, and using it here would understate a
vendor-documentation read. Keeping "from Khronos's published header" in the prose is what preserves
the strength.

## Context — this is not an isolated slip

The same invented tag was live in **six** places across the estate this morning, four of them in
`external-research/` and written by me on 2026-09-02. I have fixed all four, including this
project's own
`external-research/topics/2026-09-02b-openxr-carries-a-pose-per-view-and-the-existing-host-is-the-wrong-layer-for-m2.md`,
which is the topic §665 draws on — so once you change §665 the two will agree. Two more are in the
cross-engine library and a drop has gone to `/sr`.

Worth one note for whoever reads the sweep log: it records check 3b as *"clean estate-wide"* as of
2026-09-02, and a plain full-tree grep says otherwise. The likely cause is that 3b scans a **delta
window**, and these were written on the same day the window advanced — so "clean" currently means
"clean in the window", not "clean estate-wide". Nothing for this project to act on; flagged so the
claim is not trusted more than it should be.

## Nothing else here needs touching

The finding itself — that `XrCompositionLayerProjectionView` carries its own `pose` and `fov` per
view, so the OpenXR projection layer does not have OpenVR's single-pose collapse — is correct, is
independently recorded in two projects' topics, and is not in question.
