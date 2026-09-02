# 2026-09-02c (`/pd`, home PC, NO LAUNCH) — the M2 submission path exists, and it carries the experiment that unblocks two projects

**The game was not launched, no debugger was attached, nothing here has been run.** A code change
and a compile.

Took the remaining actionable `[PD]` item: *"M2 needs an OpenXR projection-layer host and one does
not exist."* The other XIII `[PD]` row (implement the chosen stereo route) is genuinely blocked —
it needs the `TransformRecon` log from a flat run, which does not exist yet.

---

## 1. What was missing, and why it mattered more than it looked

Both existing hosts — the OpenVR overlay and the OpenXR **quad-layer** host — are Milestone 1
designs: one flat image, the same picture to both eyes. `XrCompositionLayerQuad` is a flat rectangle
with a **single pose**, so it *structurally* cannot carry stereo.

The trap this closes: **verifying the quad host on hardware would have proved M1 over OpenXR and
absolutely nothing about M2**, while looking like progress toward it. The dossier's open risk had
been a single line reading "OpenXR host is unverified on hardware", which quietly conflated the two.

`XrCompositionLayerProjection` is the layer M2 needs — an **array** of
`XrCompositionLayerProjectionView`, each carrying **its own `pose` and its own `fov`**, all
submitted together in one layer, in one space.

## 2. What was built

`openxr_host.cpp` gains a second path behind `[VR] OpenXrProjection=1`:

- two `XrCompositionLayerProjectionView`s, pose and fov taken per-eye from `xrLocateViews`;
- **falls back to the quad layer** if fewer than two views locate, or if the view state lacks valid
  orientation *and* position — submitting a half-built projection layer would be worse than
  submitting the old one;
- config read **once outside the frame loop**, since it selects a code path rather than a value;
- one log line at startup saying which path is active, because "which layer type am I actually
  submitting" is exactly the thing that is invisible from inside a headset.

`[compile-verified 2026-09-02]` MSVC, exit 0, 186,880 bytes, **exports still 40/40** identical to the
deployed build. The only warnings are the pre-existing `_snprintf` deprecations.

## 3. ⚠️ What this is NOT — stated plainly because it would be easy to oversell

**This does not make XIII stereo.** Both projection views reference the *same* swapchain image,
because the D3D8 side does not produce two eyes yet — that is the `SetTransform` / draw-twice work
in dossier §7a, still waiting on the recon log. A mono frame shown through two per-eye frusta is
**expected to look slightly wrong**.

Its value is that it **proves the submission half of M2 independently of the render half**, so when
stereo content does arrive, the only untested thing is the content. That is a real de-risking, but
it is not the milestone.

## 4. ⭐ The part that unblocks a second project

The API *permits* independent per-view poses. Whether a given **runtime honours them** during
reprojection is untested — and SteamVR's OpenXR runtime shares a vendor with the OpenVR defect
(issue #1253) that motivated the concern in the first place.

So `[VR] OpenXrProjectionTestOffsetM=<metres>` submits the two views with deliberately **opposite**
lateral pose offsets:

- **image visibly splits between the eyes** ⇒ the runtime honours independent per-view poses, and
  the M2 submission design is sound;
- **both eyes identical however large the number** ⇒ it ignores them and reprojects from one head
  pose, which makes per-eye submission a per-runtime risk rather than a solved problem.

The offset is applied crudely, in the reference space's X, **on purpose**: the question is only
whether differing per-view poses have *any* effect, so an unambiguous split beats a physically
meaningful one.

**`far-cry-2-vr` is blocked on the identical question** for its AER submission design. One headset
test answers it for both projects.

## 5. Deployed

The staged `system\D3DDrv-m2-recon.dll` was rebuilt to include this (and mirrored to
`staging/XIII2003-vr/D3DDrv-m2-recon-2026-09-02b.dll`; md5 verified equal). **The live
`D3DDrv.dll` is still the untouched Aug-23 0.2.7 M1 build.**

The two features are independently gated, so they do not interfere: a flat `TransformRecon` run
leaves the projection code dormant because `OpenXR` defaults to 0. One DLL now serves both the flat
recon run and a later headset submission test.

`XIII.ini` gained documented `OpenXrProjection=0` / `OpenXrProjectionTestOffsetM=0` keys, both off.

## 6. What is NOT established

- **None of this has run.** It is compile-verified and staged.
- **The projection path has never touched a runtime.** The most likely first failure is not a crash
  but `xrEndFrame` rejecting the layer (a bad `fov`, a swapchain not created with the right usage
  flags, or a runtime that dislikes two views sharing one image). That would show as no image with
  the session otherwise alive — check the log line naming the active path first, so "the projection
  path is broken" is not confused with "the projection path was never enabled".
- **Two views sharing one swapchain image is legal but unusual.** If a runtime objects, the fix is a
  second swapchain, which is mechanical — but it is an assumption, not a verified behaviour.
- **The test offset proves the runtime's behaviour, not ours.** A split image says the runtime
  honours per-view poses; it says nothing about whether our eventual per-eye *content* is correct.

## 7. A concurrency problem found while working, which the user should know about

`claude-memory` in the `/pd` clone root was found with **two files in an unresolved conflict state**
(`STATUS.md`, `status/XIII2003-vr.md`) and another session's uncommitted edit to
`status/re-village-scope-vr.md`. Mid-session that file became committed and the index state changed
between two consecutive commands — i.e. **a second `/pd`-lane session was actively working in the
same clone root at the same time.**

The per-lane clone roots (`github-backups-pd/` etc.) partition by *lane*, which assumes one session
per lane. **Two concurrent `/pd` runs share a root and therefore share a working tree and an index**,
which is exactly the hazard the per-lane split was introduced to remove. My `git pull` auto-stashed
the other session's uncommitted work and the pop conflicted.

**Nothing was lost and nothing was forced.** I deliberately did not resolve, stage or commit
anything in `claude-memory` — fighting a live session over the same files is how work disappears —
and the XIII code, dossier and notes all live in repos the other session was not touching. **The
status-board entry for this work is therefore deferred**, and is the one thing still owed.
