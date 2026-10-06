# 2026-10-06: one keypress, and the twelve mystery draws were never draws

Dev PC, `/lm`, auto-picked. One launch straight into the bank (`XIII.exe Banque01`), stereo switched
on with numpad 7, then one press of numpad `.`.

## What the log said

- **Nothing in the bank is drawn in screen space.** The dump of the next frame found zero
  pre-transformed draws. The "twelve" from September were shader tickets whose number happened to look
  like the screen-space flag, which the 2026-09-13 fix now ignores (4 of them per frame in today's view).
  So the HUD depth is no longer being applied to ordinary walls and floors, and that is now a measured
  fact rather than a static reading. `[verified-live 2026-10-06, n=1]`
- **The ticket census** lists 12 distinct values: 2 real layout codes, 6 declaration-only tickets,
  4 programmable shaders. Ticket `0x15` is the one that used to be misread. `[verified-live 2026-10-06, n=1]`
- **The third question did not get a clean answer.** The counter meant to say whether the HUD's own
  group of draws was affected the same way counts something broader (every time such a ticket is set, not
  just for HUD draws). It read 12 per frame, which therefore proves nothing about the HUD. That question
  went back to static work. `[inferred-static 2026-10-06]`
- **The HUD shows in both eyes**, crosshair centred in each. Screenshot in the recon folder.
- **The "140 render-target switches per frame" mystery was a units slip.** That number is a per-second
  total; per frame it is exactly 2. The background helper found it in the code and in 14,900 old log lines,
  and today's run agrees on every line. `[verified-live 2026-10-06, n=6 lines]`

## Housekeeping done

- Music set to zero in `XIII.ini` and `User.ini` (backed up). Not yet seen in a running game.
- Learned how to quit cleanly with the keyboard (two menus, two Yes prompts). Closing the window and
  typing `exit` in the console both do nothing.
- The game deletes `User.ini` on exit; it was identical to `DefUser.ini`, so it was simply put back.

## Not established

- Which two passes the 2 render-target switches per frame belong to.
- Whether the profile's saved music slider overrides the ini on the next launch.
- Anything about the headset: the `[VR USER]` row is unchanged.

Evidence: `dev-archive/recon/2026-10-06-numpad-dot-the-twelve-were-never-draws/`. Dossier §11j and §4.
