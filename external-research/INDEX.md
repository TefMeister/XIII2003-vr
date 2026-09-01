# Research index

**Last `/gr` pass: 2026-09-01 — FULL.** Inbox was empty. One new topic, and it does two things: it
**narrows an over-broad claim of our own** (a public Epic-hosted UE2 doc set *does* exist; it is the
C++ headers that are NDA-gated, which is the half that actually blocks us), and it supplies
first-party corroboration for §7a — Epic's own page records that UE2's native render-driver headers
were used to build **"360 degree rendering drivers for VR systems"**, i.e. the exact layer our
`FD3DRenderInterface::SetTransform` hook sits in. Nothing here unblocks the project; the missing
proxy source tree is still the only blocker.

Every research topic gathered for this project, newest first. Each row links to a self-contained
write-up in `topics/`. Status tags:

- 🆕 **new** — found, not yet acted on by the modding side.
- 👀 **reviewed** — a modding session has read it and factored it into a decision, but nothing shipped from it yet.
- ✅ **incorporated** — directly led to a real change (code, a test, a note) in one of the other five repos; linked below.
- ❌ **dead end** — checked out, didn't pan out; kept for the record so it isn't re-investigated from scratch.

| Date | Topic | Status | Summary |
| --- | --- | --- | --- |
| 2026-09-01 | [UE2's docs are public, its headers are NDA — and Epic says VR drivers were built at our hook layer](topics/2026-09-01-ue2-docs-are-public-but-the-headers-are-nda-and-epic-says-vr-drivers-were-built-here.md) | 🆕 new | Corrects the row below: an Epic-hosted UE2 documentation set is live at `docs.unrealengine.com/udk/Two/` (UnrealScript reference, runtime topics, `UnDox`), but the **C++ native headers are available only under NDA** — so `FRenderInterface`/`ETransformType` were never publicly documented and §7a's hand decoding had no shortcut. The find: Epic's own `RuntimeHeaders` page names **"360 degree rendering drivers for VR systems"** among past uses of those headers — the render-device layer we already replace is the documented seam for this work. Also: UT2004 under vorpX must be switched to the **OpenGL** renderer, and no UE2 render-device stereo prior art exists at all. |
| 2026-08-25 | [Native F2 console: Fly/Ghost/God commands](topics/2026-08-25-native-f2-console-fly-ghost-commands.md) | ✅ incorporated | Confirmed live and **built into shipped tooling** — the 0.2.9 automation harness drives these commands from outside the process (`HealMe 100` raised health 50→100 from a text file); dossier §9a. Command list corrected: `Teleport`/`AllAmmo`/`Loaded`/`Invisible`/`SetSpeed`/`ChangeSize`/`Slomo` are **absent** from this build; `Fov`/`BehindView` are present, on the PlayerController. The page's own "standard UE command therefore present" reasoning is **disproved** — this build ships a subset. Its follow-up question (a teleport equivalent) is **closed 2026-08-28**: navigation was solved via keyboard input and XIII's own *unbound* turn aliases, no command needed. |
| 2026-08-25 | [UE Explorer decompiler + UE2 public-doc gap](topics/2026-08-25-ue-explorer-decompiler-and-ue2-doc-gap.md) | 🆕 new | UE Explorer is the confirmed right tool for the dossier's own named Milestone 2 lever (decompiling `.u` packages to find weapon-aim UnrealScript functions). Honest gap noted: unlike UE3/Alice, no public UE2 SDK/doc equivalent exists — native camera/projection work stays this project's own live derivation. **⚠️ Narrowed 2026-09-01: a public Epic-hosted UE2 *documentation* set does exist; it is the C++ headers that are NDA-gated.** The conclusion is unchanged, the reason is sharper, and UnrealScript-level material — the layer the M2 lever works at — turns out to be public. See the 2026-09-01 row above. |

## How to add a topic

1. New file in `topics/`, named `YYYY-MM-DD-short-slug.md`.
2. One row added to the table above, newest at the top.
3. Update the status tag here as it moves through review → incorporated/dead-end (the modding side should update this when it acts on a lead, so the index reflects reality without the research side needing to poll).
