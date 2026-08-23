# 2026-08-23 — Home-machine perf A/B: readback is CHEAP; GPU-only path deprioritized; M2 is the answer to smoothness

0.2.7 (staged prebuilt DLL) deployed to the home PC and run with a real
Quest 3 (Virtual Desktop → SteamVR). Raw log archived beside this entry as
`perf-2026-08-23-home-quest3.log`.

## Numbers (synchronous readback, cap 11 ms, pipe=0)

- Game presented **~65 fps sustained** (324–328 presents / 5 s), captured
  ~60/s, ~6% frames skipped by the 11 ms cap — the limiter working as designed.
- Per-frame VR overhead averages: **copy (CopyRects GPU→CPU) ≈ 1.5–1.6 ms**
  (max ~6 ms, one 15 ms outlier), lock ≈ 0, decode ≈ 0.9 ms, submit ≈ 0.45 ms.
  **Total ≈ 3 ms/frame** — comfortably inside a 15.4 ms budget.

## Verdict

**The CPU readback is affordable. The GPU-only shared-surface path drops from
"the remaining lever" to nice-to-have.** (Same ruling as Far Cry 2's bridge
the same day — CPU readback held up fine there too.) Dev-PC effort should go
to **Milestone 2** (native stereo + motion-controlled aim) instead of
readback plumbing.

## User experience notes (same session)

- **"Runs fine, just not really nice and smooth"** — diagnosis: M1's
  architectural ceiling, not the game (it holds 65 fps easily): the flat
  overlay updates at ~60–65 while the HMD refreshes at 72/90 → cadence
  judder; head-look routes HMD pose → hooked mouse-look → next game frame → a
  frame or two of added lag. Both are inherent to M1 and both are removed by
  M2's native path. Optional mitigation to try someday: VD at 120 Hz (60
  divides evenly).
- **0.2.6 focus-hook side effect, worth documenting:** the game no longer
  freezes when unfocused — but if another window steals focus, the game keeps
  rendering while INPUT goes elsewhere, which reads as "the game froze on me"
  (user's words) even though heartbeats run perfectly. Fix: click the game
  window. Consider a future QoL: reassert focus or show an on-overlay hint
  when input focus is lost.
- Deploy note: the home install had an Aug-19 pre-perf-logging build; the
  staged 0.2.7 DLL from this repo's sibling staging repo deployed cleanly
  (old DLL kept as `D3DDrv.dll.pre-027-bak`).
