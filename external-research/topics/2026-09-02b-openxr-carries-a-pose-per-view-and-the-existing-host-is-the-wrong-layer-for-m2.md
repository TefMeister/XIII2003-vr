# OpenXR carries a pose per view — and this project's existing OpenXR host is the wrong layer type for M2

**Status:** 🆕 new · **Priority:** high — it verifies a claim filed as a hypothesis an hour ago on a
sibling project, and it identifies a gap in this project's own §7 that would otherwise be found the
expensive way, during M2.

## The claim, now verified rather than argued

`far-cry-2-vr` filed a lead earlier today: OpenVR's per-eye pose mechanism is broken (SteamVR keeps
only the pose from the last `Submit` call — issue #1253, still open after seven years), and **OpenXR's
projection layer should not have the same problem because it carries a pose per view.** That was
tagged `[hypothesis]`, reasoned from the API's shape, with a note that it needed checking against the
specification before anyone built on it.

**Checked, from Khronos's own published header** `[verified-static 2026-09-02]`:

```
XrCompositionLayerProjectionView {
    XrStructureType       type;
    const void*           next;
    XrPosef               pose;      // this view's own pose
    XrFovf                fov;       // this view's own field of view
    XrSwapchainSubImage   subImage;
}

XrCompositionLayerProjection {
    XrStructureType                         type;
    const void*                             next;
    XrCompositionLayerFlags                 layerFlags;
    XrSpace                                 space;
    uint32_t                                viewCount;
    const XrCompositionLayerProjectionView* views;   // an array
}
```

(Read alongside `XrPosef` and `XrFovf` as controls, so the fetch demonstrably reached the right region
of the file rather than returning a plausible-looking miss.)

**The structural difference is real.** Both eyes are handed over **together, inside one layer
structure, in one space**, each carrying its own pose and its own field of view. There are no
separate per-eye submission calls, so the "last call overwrites the first" collision that produces
OpenVR #1253 has nowhere to occur. Per-eye poses are expressible in OpenXR by construction.

**⚠️ What this does not establish.** That the API *can* express per-view poses is now a fact. That any
given runtime *honours* them independently during reprojection is a separate, empirical question —
and SteamVR's OpenXR runtime comes from the same vendor whose OpenVR path has the unfixed bug. So the
correct reading is: **the design is no longer blocked by the API, and the remaining risk moved from
"impossible" to "untested per runtime."** `[reported]` for runtime behaviour.

## ⭐ The part that is specific to this project, and is a gap

Dossier §7 records two host paths: an **OpenVR overlay** (hardware-verified) and an **OpenXR
quad-layer host** (unverified on hardware). Both are M1 designs — they present the finished 2D
framebuffer as a flat panel in space.

**A quad layer cannot carry stereo.** `XrCompositionLayerQuad` is a flat rectangle with a single
pose; it is the right tool for M1's "one image, both eyes" and the wrong tool for M2. True per-eye
rendering needs the **projection layer** above, with one view per eye.

So the queued §12 item *"OpenXR host is unverified on hardware"* is, for M2 purposes, understated.
Verifying the quad-layer host would confirm M1 works over OpenXR; **M2 needs a projection-layer path
that does not exist yet.** That is not a large piece of work — the swapchain and session handling are
shared — but it is a different code path, and knowing that now is better than discovering it after the
`SetTransform` hook starts producing two eyes with nowhere to send them.

## How this fits the M2 plan already on the board

M2's stereo hook produces two per-eye views inside the render device. Where those two images go is a
separate decision, and it now has a clear shape:

| Route | Per-eye images | Per-eye poses |
| --- | --- | --- |
| OpenVR overlay (M1's, hardware-verified) | not supported — one flat panel | n/a |
| OpenVR `Submit` per eye | yes | **no** — #1253 discards all but the last |
| **OpenXR projection layer** | yes | **yes, by construction** — runtime honouring still untested |

Nothing here changes the immediate next step, which remains the one recon launch with
`TransformRecon=1`. It changes what the submission side should be built as once that lands.

## Concrete next steps

1. Keep the recon launch as the next action; this is about the step after it.
2. When M2 submission is designed, target **`XrCompositionLayerProjection` with two views**, not the
   existing quad-layer host — and treat the quad host as M1's, kept for the flat path.
3. Worth one cheap empirical check when a headset session happens: submit two views with deliberately
   different poses and confirm the runtime honours both rather than collapsing them. That answers the
   remaining question for this project **and** for `far-cry-2-vr`, which is waiting on the same fact.

## Sources

- https://github.com/KhronosGroup/OpenXR-SDK — `include/openxr/openxr.h`, the struct definitions above (read online; described in our own words, nothing copied)
- https://github.com/ValveSoftware/openvr/issues/1253 — the OpenVR per-eye pose defect, re-checked 2026-09-02 and still open
- `far-cry-2-vr/external-research/topics/2026-09-02-the-steamvr-per-eye-pose-bug-is-still-open-and-openxr-is-the-way-around-it.md` — the sibling topic this verifies
