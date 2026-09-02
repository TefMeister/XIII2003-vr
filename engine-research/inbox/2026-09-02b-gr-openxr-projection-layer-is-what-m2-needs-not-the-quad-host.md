# §7/§12: M2 needs an OpenXR **projection** layer — the existing quad-layer host is an M1 design and cannot carry stereo

Filed by: `/gr`, 2026-09-02
Topic: `external-research/topics/2026-09-02b-openxr-carries-a-pose-per-view-and-the-existing-host-is-the-wrong-layer-for-m2.md`
Dossier sections: §7 (the two host paths), §12 ("OpenXR host is unverified on hardware")

`[verified-static 2026-09-02, from Khronos's published `openxr.h`, read with control structs alongside]`

- **`XrCompositionLayerProjectionView` carries its own `pose` and its own `fov`**, and `XrCompositionLayerProjection` holds an **array** of them (`viewCount` + `views`) submitted **together in one layer, in one space**. So per-eye poses are expressible by construction — there are no separate per-eye submit calls, and therefore none of the "last call wins" collision behind OpenVR #1253 (re-checked today: still open, seven years, no Valve response).
- **⚠️ Runtime honouring is a separate question.** The API allows independent per-view poses; whether a given runtime respects them during reprojection is untested, and SteamVR's OpenXR runtime shares a vendor with the OpenVR defect. Risk moved from "impossible" to "untested per runtime". `[reported]`
- **⭐ The gap in §7:** both existing hosts are **M1** designs presenting one flat image — the OpenVR overlay and the OpenXR **quad-layer** host. `XrCompositionLayerQuad` is a flat rectangle with a single pose and **cannot carry stereo**. Verifying the quad host proves M1 over OpenXR; **M2 needs a projection-layer path that does not exist yet.** Session and swapchain handling are shared, so it is not large — but it is a different code path, and better known before the `SetTransform` hook starts producing two eyes.

Suggested dossier changes: in §7, label the quad-layer host explicitly as the M1 flat path; in §12, split the open item into "quad host unverified on hardware (M1)" and "no projection-layer path exists yet (M2)". Nothing here changes the next action, which is still the one recon launch with `TransformRecon=1`.

Cross-project: `far-cry-2-vr` is waiting on the same runtime question for its AER submission design. A single headset test — two views submitted with deliberately different poses — answers it for both projects.
