# UE2's documentation *is* public — its C++ headers are not — and Epic's own page says VR rendering drivers were built at exactly our hook layer

**Status:** 🆕 new · **Priority:** medium-high — it corrects an over-broad claim in our own lane, and
it supplies first-party corroboration for §7a, which is currently `[inferred-static, n=1, never run]`
and blocked from being tested.

## Correcting our own earlier claim

`topics/2026-08-25-ue-explorer-decompiler-and-ue2-doc-gap.md` records, and `INDEX.md` repeats:

> unlike UE3/Alice, **no public UE2 SDK/doc equivalent exists**

That is **too broad as written**, and precisely right on the part that matters. Both halves are worth
separating, because the second half is what actually blocks us.

**A public, Epic-hosted UE2 documentation set does exist** `[reported 2026-09-01]` — the old Unreal
Developer Network material, still served at `docs.unrealengine.com/udk/Two/`. Live pages include:

| Page | What it is |
| --- | --- |
| `UnrealScriptReference` | the UnrealScript language reference |
| `RuntimeTopics` | generated index of all UE2 Runtime topics |
| `UnrealEngine2Runtime22262002` | level-design process and **rendering configuration options** |
| `UTModHome` | working with the engine technically — building games/mods, networking, rendering |
| `UnrealEdInterface` | the editor interface |
| `UnDox` | Epic's own tool that turns UnrealScript into HTML documentation |
| `RuntimeHeaders` | an **introduction to** the C++ native source headers |

**But the headers themselves are NDA-gated.** The `RuntimeHeaders` page describes the C++ native
source headers for the UE2 Runtime — the ones that would contain `FRenderInterface`, `ETransformType`
and the `TT_*` constants — and states they are **available only under non-disclosure agreement**
`[reported 2026-09-01]`. So there is no public place to look up the very declarations §7a decoded by
hand, and there never was.

**Net effect on the lane:** the *conclusion* stands unchanged — native camera/projection work on this
project stays our own live derivation, because the C++ interface is not public. What changes is the
reason, and one practical consequence: **UnrealScript-level material is public and reachable**, which
is the layer the Milestone 2 lever (decompiling `.u` packages for weapon-aim functions) actually works
at. `UnDox` is Epic's own tool for that job and is worth knowing about beside UE Explorer.

⚠️ **Fetch caveat, stated because it changes how much weight this carries.** Every direct fetch of
`docs.unrealengine.com/udk/Two/*` returned **HTTP 403** — the pages are indexed and their content is
summarised by search, but this pass could not read one directly. The page *titles* and the NDA
statement are well-attested across multiple results; treat the table above as a reliable map and
anything finer as unverified until someone opens the pages in a browser.

## ⭐ The corroboration §7a deserves

The same `RuntimeHeaders` description says what licensees historically *did* with those native
headers. Among the named past uses `[reported 2026-09-01]`:

> **360 degree rendering drivers for VR systems**

— alongside motion-capture device interfaces and integrations with other software.

That is Epic, describing UE2's own render-driver interface as the documented extension point at which
**VR and multi-view rendering drivers were built**, in this engine generation, by people who had the
headers.

§7a's hook — `FD3DRenderInterface::SetTransform` at `D3DDrv.dll + 0x129E0`, the single funnel through
which `TT_LocalToWorld` / `TT_WorldToCamera` / `TT_CameraToScreen` reach
`IDirect3DDevice8::SetTransform` — sits inside exactly that render-device layer. The mod already
replaces `D3DDrv.dll`, which is to say **it already occupies the seam Epic documented as the one for
this kind of work.**

This does not verify a single address, and it must not be read as doing so: §7a stays
`[inferred-static 2026-09-01, n=1]` until it runs. What it does is remove a category of doubt —
*"are we hooking at a sane layer, or fighting the engine's architecture?"* The answer is that this is
the layer the engine was extended at.

## Two smaller UE2-family notes

- **The generic-driver route needs OpenGL, not D3D.** For UT2004 — the flagship UE2 title — running
  under vorpX requires manually switching `RenderDevice` in the `.ini` from the D3D renderer to the
  **OpenGL** one `[reported 2026-09-01]`. Two things follow. First, it is a reminder that UE2's
  renderer is *selectable at the ini level*, which is a cheap A/B lever for isolating renderer-specific
  behaviour. Second, and more useful: the generic-driver ecosystem evidently could **not** work with
  UE2's D3D path and had to route around it — which is a mild argument that our own D3D8 hook is doing
  something the off-the-shelf tools do not, rather than duplicating them.
- **No UE2 stereo/VR prior art at the render-device level was found.** Searching for per-eye or
  stereoscopic UE2 work returns UEVR and the modern UE4/UE5 ecosystem almost exclusively — UEVR hooks
  Unreal's *own built-in stereo path*, which UE2 does not have. This is a genuine gap, not a failed
  search: this project is doing something with no public precedent in its engine generation.

## Concrete next steps

1. **When someone is at a browser**, open `docs.unrealengine.com/udk/Two/RuntimeTopics.html` and
   `UTModHome.html` directly — automated fetches are 403ed, and the rendering-configuration and
   technical pages may hold ini-level detail worth adding to §9.
2. **Nothing here unblocks §7a** — the blocker remains the missing proxy source tree, not knowledge.
   This topic exists so that when the source is recovered, the hook is started with confidence rather
   than with a doubt about the layer.

## Sources

- https://docs.unrealengine.com/udk/Two/RuntimeHeaders.html (403 on fetch; search-summary only)
- https://docs.unrealengine.com/udk/Two/RuntimeTopics.html
- https://docs.unrealengine.com/udk/Two/UnrealScriptReference.html
- https://docs.unrealengine.com/udk/Two/UTModHome.html
- https://docs.unrealengine.com/udk/Two/UnDox.html
- https://docs.unrealengine.com/udk/Two/UnrealEngine2Runtime22262002.html
- https://www.vorpx.com/forums/topic/unreal-tournament-2004/
