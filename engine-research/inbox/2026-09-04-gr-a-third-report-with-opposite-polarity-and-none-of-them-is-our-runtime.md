# §12: a third first-hand report, with the opposite polarity — and none of the three is the runtime we will actually test on

Filed by: `/gr` (estate sweep, third pass 2026-09-04), for the modding lane.
Section: `ENGINE-DOSSIER.md` §12 → "Not merely untested — known to VARY between runtimes and versions"
Sibling topic: `far-cry-2-vr/external-research/topics/2026-09-02-the-steamvr-per-eye-pose-bug-is-still-open-and-openxr-is-the-way-around-it.md` (§"Corrected and extended 2026-09-04")

## What §12 already has, and what this adds

§12 records two contradicting first-hand reports — LukeRoss00 (2020-10, **OpenXR**, Valve Index:
per-view poses wrong, *Oculus and Microsoft correct*) and SirKandela with Rylie Pavlik (2023-09:
*Oculus desktop runtime* appears to ignore the submitted view pose, SteamVR respects it). Its
conclusion, that the behaviour varies by runtime and version and the runtime must be recorded, is
correct and is strengthened rather than changed by what follows.

**The third data point, from the OpenVR side, has the opposite polarity to LukeRoss's own OpenXR
report.** On OpenVR issue #1253, on 2019-12-17, the same author wrote `[reported 2026-09-04]`:

> "Apparently this problem was fixed in one of the latest betas, but only for the lighthouse driver
> (not for the Oculus and WMR backends)."

Reiterated 2019-12-29, hedged both times, never in a public changelog.

So the same person reported, ten months apart: **on OpenVR, lighthouse fixed and Oculus/WMR broken;
on OpenXR, Index broken and Oculus/Microsoft correct.** Different APIs, so these are not strictly
contradictory — but the polarity flip is exactly why **an OpenVR-era observation must not be carried
across to reason about the OpenXR path**, in either direction. §12's "varies by runtime *and
version*" now has three points behind it instead of two, and one of them is a same-author reversal.

## ⚠️ The part that should reach the `[VR]` row's outcome table

**None of the three public reports is about the runtime this project will actually be tested on.**
In-headset testing happens on the home PC's **Quest 3 over Virtual Desktop (VDXR / SteamVR)**
(`claude-memory/MACHINES.md`). That is:

- not a **Valve Index / lighthouse** device (LukeRoss's OpenXR failure case, and the OpenVR
  fixed-case),
- not the **Oculus desktop runtime** (SirKandela's failure case),
- but **VDXR**, a third OpenXR runtime with no public report in evidence either way.

The practical consequence for the row's outcome table: **do not expect any of the three reported
signatures specifically, and do not read their absence as a pass.** The row's existing instruction —
look for a visibly wrong stereo baseline together with vertical misalignment, which is a *positive*
identification — remains exactly the right test, because it does not depend on which runtime is at
fault. Keep it, and record `VDXR` plus its version beside whatever it produces, since that result
will be the first public data point for this runtime that any of these threads has.

## One small correction available while you are in there

§12's OpenXR paragraph cites #1253 as *"re-checked 2026-09-02: still open, seven years, no Valve
response"*. Still open and no Valve response are both confirmed comment by comment
`[verified-live 2026-09-04, n=1 API read]`. If a precise date is ever wanted, the issue's **last
activity is 2020-04-22**, not its 2019-11-23 creation date — the equivalent clause in
`far-cry-2-vr`'s dossier stated the creation date as the last activity and is being corrected there.

## Source

- https://github.com/ValveSoftware/openvr/issues/1253 — metadata and all 8 comments, read via the
  GitHub API on 2026-09-04.
