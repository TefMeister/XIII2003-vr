# The twelve "invisible" draws were probably never what we thought (2026-09-13, dev PC, `/pd`)

**One sentence:** the twelve mystery draws the HUD depth was being applied to are almost certainly
not screen-space draws at all — we were reading a number wrong — and the build now says so out loud
the moment you launch it.

## What the question was

Since 2026-09-11 the log has reported **12 "pre-transformed" draws per frame** in the bank level —
draws whose positions are already in screen pixels, which is how a HUD is normally drawn. The per-eye
HUD fix duly moved all twelve into each eye. **But nothing on screen ever looked like those twelve.**
So we were moving *something* to HUD depth without knowing what it was, which is exactly the kind of
thing that quietly looks wrong in a headset.

## What I found, by reading the game's own files

Two independent readings, and they agree:

1. **The game engine has no screen-space drawing path at all.** Every one of its fourteen kinds of
   geometry describes its positions as ordinary 3D, not as screen pixels. Not one is the exception
   `[inferred-static 2026-09-13]`.
2. **Nor does the game's renderer.** The only two "vertex layout" numbers it ever hands the graphics
   card directly are both ordinary 3D ones — the normal world geometry and the one the HUD/menu
   drawing uses `[inferred-static 2026-09-13]`.

So if neither the engine nor its renderer ever asks for screen-space vertices, **the twelve cannot be
what the counter said they were.**

## What was actually happening

The number we were reading is a **dual-purpose number**. The graphics card's "which layout am I
using" setting accepts either a *description* of the layout, or a *ticket* referring to one the
renderer registered earlier. They arrive down the same wire, and the only way to tell them apart is
to have watched the ticket being issued.

We were watching — but only for one of the two kinds of ticket, and the game's renderer issues the
other kind `[inferred-static 2026-09-13, read out of the renderer's own code]`. So those tickets
were being read as descriptions. **Roughly one ticket number in eight happens to read as
"screen-space".** Twelve draws a frame is what that looks like.

There was a second, quieter version of the same bug: the list of tickets we did watch for held only
**64** of them, and silently stopped recording after that. Anything created later was misread too.

## What changed

- Both kinds of ticket are now recorded, and the list holds **512**, and it **says so in the log** if
  it ever fills instead of going quiet.
- A draw using a ticket is **no longer counted as screen-space**, whatever its number looks like.
- ⚠️ **Deliberately surgical.** That is the *only* behaviour that changed. The per-eye HUD — which is
  verified in the headset and shipped in v0.3.0-alpha — reads the same number through a different
  door, and that door was left exactly as it was. If it turns out to have the same problem, the new
  log line below says so **without anything moving**.
- New log lines: **one line per distinct layout value ever used**, saying whether it was a ticket we
  watched being issued or a real description, and what it would have decoded to. Plus two new
  counters: how many draws were rescued from the wrong bucket, and whether the HUD bucket has the
  same issue.
- **Numpad `.`** now describes every genuinely screen-space draw of the next frame: its size and
  position on screen, the texture it uses and how it is blended. (Numpad `.` is unbound in the game —
  checked.)

## What a launch says, in one press

Launch the flat stereo build, get into the bank, press **numpad `.`** once, and the log answers three
things at once:

| what the log shows | what it means |
| --- | --- |
| `rhw-suppressed=12` (or thereabouts) and **nothing** after `RHW DUMP` | ✅ **settled: the twelve were never draws.** The HUD depth was being applied to ordinary wall-and-floor geometry, and now isn't. The mystery closes and one real bug closes with it. |
| `RHW DUMP` lines describing small boxes out in the scene | there ARE real screen-space draws, and they are light glows or flashes — they want the *lamp's* depth, not the HUD's, so the HUD depth must stop touching them |
| `RHW DUMP` lines at a fixed screen corner | they are HUD after all, and the current behaviour was right |
| `decl-xyz=` greater than zero | ⚠️ the per-eye HUD bucket has the same misreading. **Nothing has moved** — but it needs the same fix, and this is how we would know. |
| `VERTEX SHADER TABLE FULL` | the 512 list filled; tell me and I will raise it |

## Proved, and not proved

- The engine-and-renderer reading is **static**: read out of the shipped files, nothing run
  `[inferred-static 2026-09-13]`.
- The code change is **compile-verified and covered by tests**: the fake-graphics-card test suite
  grew a case that models the exact hazard — a ticket whose number reads as "screen-space" — and
  checks that the draw's positions are **not** rewritten, plus a companion check that a *genuinely*
  screen-space draw still is, so the first check cannot pass by the whole path being dead.
  **57 checks with the fixes on, 43 with them off, 0 failures**, plus the three maths suites
  unchanged at 14/11/12 cases `[compile-verified 2026-09-13]`.
- ⚠️ **Not proved: that the twelve specifically were tickets.** The reasoning is strong and the two
  static readings agree, but the log line above is what turns it into fact. One press of one key.

## Gate

`GATE: PD` still, but only just — the two remaining static items on this game are both marked
"demoted". Everything interesting now needs the game up, and this build makes one launch answer far
more than it did this morning.
