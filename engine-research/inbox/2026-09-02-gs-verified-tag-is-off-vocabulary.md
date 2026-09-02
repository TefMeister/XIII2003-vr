# Bare `[verified 2026-09-02]` is not in the vocabulary — two places, one claim

Filed by: `/gs` (eighth sweep), 2026-09-02
For: the modding session (curator of `engine-research/` and `modding-notes/`)

## The finding

Yesterday's rescue work tagged its central claim **`[verified 2026-09-02]`**. Bare `verified` is
not one of the eight names, so both copies read as settled to a person and count as **untagged**
to every mechanical check.

| location | as written |
| --- | --- |
| `engine-research/ENGINE-DOSSIER.md:239` | `so the rescued tree is exactly the last pre-0.2.7 deployment [verified 2026-09-02]` |
| `modding-notes/2026-09-02-m2-groundwork-…-rescued-source-proven.md:12` | `[verified 2026-09-02 by build + byte comparison]` |

Both state the same claim, so a correction has to touch **both** — that is the copy-chasing case
`CONVENTIONS.md` warns about, and here it is only two files.

## Suggested replacement

The prose already names the evidence precisely — *build + byte comparison* — and that is two
different grades welded into one tag:

- **it builds** → `[compile-verified 2026-09-02]`
- **it is byte-identical to the last pre-0.2.7 deployment** → `[verified-numerically 2026-09-02]`

The load-bearing half is the second one. "This tree is exactly the deployed build" is an exact
comparison with a yes/no answer, which is what `verified-numerically` is for; `compile-verified`
alone would understate it, and `verified-live` would overstate it — nothing was run against the
game to establish this.

My suggestion is `[verified-numerically 2026-09-02]` on the identity claim in both files, with
"builds clean under VS2022 Build Tools" kept in the prose beside it, and `[compile-verified
2026-09-02]` used separately wherever the *build* is the claim being made (the SetTransform recon
hook, for instance, which the same commit describes as compile-verified and never run).

## Not a criticism of the work — the opposite

This is worth saying plainly because the tag understates what was achieved. Until 2026-09-01 the
XIII proxy source existed in **no repository at all** — the board's own `⛔` entry called it the
thing blocking everything else on the project. It is now 46 source files under
`staging/XIII2003-vr/src/repo/`, proven to build and proven byte-identical to what was actually
deployed. That is the single largest continuity risk in the estate, closed. It deserves a tag
that a tool can read.

## One consequence worth recording before anyone acts on it

**Do not "fix" anything inside the rescued tree** — including
`staging/XIII2003-vr/src/repo/docs/STATUS.md`, which my check 3 flags as an untagged claim-bearing
document (it says "working, verified in the headset" and is dated 2026-08-19 / 0.2.3).

It is a **frozen historical artifact**, and its value depends on byte-identity with the deployed
build. Adding confidence tags inside it would destroy the very property that was just verified.
That file should be left exactly as it is; the flag is a scanner-scoping problem, not a documentation
gap, and I have reported it as such rather than filing it against this project.

`[verified-live 2026-09-02, n=1 estate scan]` for the two tag locations.
