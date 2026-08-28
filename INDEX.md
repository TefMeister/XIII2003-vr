# Research index

Every research topic gathered for this project, newest first. Each row links to a self-contained
write-up in `topics/`. Status tags:

- 🆕 **new** — found, not yet acted on by the modding side.
- 👀 **reviewed** — a modding session has read it and factored it into a decision, but nothing shipped from it yet.
- ✅ **incorporated** — directly led to a real change (code, a test, a note) in one of the other five repos; linked below.
- ❌ **dead end** — checked out, didn't pan out; kept for the record so it isn't re-investigated from scratch.

| Date | Topic | Status | Summary |
| --- | --- | --- | --- |
| 2026-08-25 | [Native F2 console: Fly/Ghost/God commands](topics/2026-08-25-native-f2-console-fly-ghost-commands.md) | ✅ incorporated | Confirmed live and **built into shipped tooling** — the 0.2.9 automation harness drives these commands from outside the process (`HealMe 100` raised health 50→100 from a text file); dossier §9a. Command list corrected: `Teleport`/`AllAmmo`/`Loaded`/`Invisible`/`SetSpeed`/`ChangeSize`/`Slomo` are **absent** from this build; `Fov`/`BehindView` are present, on the PlayerController. The page's own "standard UE command therefore present" reasoning is **disproved** — this build ships a subset. Its follow-up question (a teleport equivalent) is **closed 2026-08-28**: navigation was solved via keyboard input and XIII's own *unbound* turn aliases, no command needed. |
| 2026-08-25 | [UE Explorer decompiler + UE2 public-doc gap](topics/2026-08-25-ue-explorer-decompiler-and-ue2-doc-gap.md) | 🆕 new | UE Explorer is the confirmed right tool for the dossier's own named Milestone 2 lever (decompiling `.u` packages to find weapon-aim UnrealScript functions). Honest gap noted: unlike UE3/Alice, no public UE2 SDK/doc equivalent exists — native camera/projection work stays this project's own live derivation. |

## How to add a topic

1. New file in `topics/`, named `YYYY-MM-DD-short-slug.md`.
2. One row added to the table above, newest at the top.
3. Update the status tag here as it moves through review → incorporated/dead-end (the modding side should update this when it acts on a lead, so the index reflects reality without the research side needing to poll).
