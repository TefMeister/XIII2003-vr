# Research index

Every research topic gathered for this project, newest first. Each row links to a self-contained
write-up in `topics/`. Status tags:

- 🆕 **new** — found, not yet acted on by the modding side.
- 👀 **reviewed** — a modding session has read it and factored it into a decision, but nothing shipped from it yet.
- ✅ **incorporated** — directly led to a real change (code, a test, a note) in one of the other five repos; linked below.
- ❌ **dead end** — checked out, didn't pan out; kept for the record so it isn't re-investigated from scratch.

| Date | Topic | Status | Summary |
| --- | --- | --- | --- |
| 2026-08-25 | [Native F2 console: Fly/Ghost/God commands](topics/2026-08-25-native-f2-console-fly-ghost-commands.md) | 🆕 new | A real native console (F2) exists with stock Unreal cheat-manager commands (God/Fly/Ghost/PlayersOnly/etc), not yet in the dossier — Fly/Ghost in particular are directly useful for fast, collision-free stereo-depth and aim testing during Milestone 2. |
| 2026-08-25 | [UE Explorer decompiler + UE2 public-doc gap](topics/2026-08-25-ue-explorer-decompiler-and-ue2-doc-gap.md) | 🆕 new | UE Explorer is the confirmed right tool for the dossier's own named Milestone 2 lever (decompiling `.u` packages to find weapon-aim UnrealScript functions). Honest gap noted: unlike UE3/Alice, no public UE2 SDK/doc equivalent exists — native camera/projection work stays this project's own live derivation. |

## How to add a topic

1. New file in `topics/`, named `YYYY-MM-DD-short-slug.md`.
2. One row added to the table above, newest at the top.
3. Update the status tag here as it moves through review → incorporated/dead-end (the modding side should update this when it acts on a lead, so the index reflects reality without the research side needing to poll).
